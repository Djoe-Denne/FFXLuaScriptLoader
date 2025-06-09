#!/usr/bin/env python3
"""
Script de pré-patching d'instructions pour ff8_hook
=====================================================

Ce script prend un CSV (adresse, instruction) extrait d'IDA et génère un fichier TOML
avec les bytes d'instructions où les adresses sont marquées avec 'X' pour indiquer
les bytes à patcher, accompagnés des offsets calculés.

Le script désassemble les instructions et identifie les bytes contenant des adresses
mémoire qui doivent être relocalisées vers une nouvelle base mémoire.

Usage: python patch_memory_usage.py --csv <file> --binary <file> [options]

Exemple:
    python patch_memory_usage.py --csv ida_usage.csv --binary FF8_EN.exe --memory-base 0x01CF4064

Sortie TOML:
    [instructions]
    "0x0048D774" = {bytes = "8D 86 XX XX XX XX", offset = "0x2A"}
    "0x0048D8A4" = {bytes = "8D 90 XX XX XX XX", offset = "0x2A"}
"""

import csv
import struct
import logging
import os
import re
import argparse
import toml
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

def setup_logging(log_file, verbose):
    """Configure le système de logging"""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file),
            logging.StreamHandler(sys.stderr)  # Logs vers stderr pour éviter de polluer le TOML
        ]
    )
    return logging.getLogger(__name__)

def parse_arguments():
    """Parse les arguments de ligne de commande"""
    parser = argparse.ArgumentParser(
        description="Désassemble des instructions à partir d'adresses CSV et remplace les adresses par des offsets"
    )
    
    parser.add_argument(
        "--csv", 
        required=True,
        help="Fichier CSV contenant les adresses à désassembler"
    )
    
    parser.add_argument(
        "--binary", 
        required=True,
        help="Fichier binaire à analyser (ex: FF8_EN.exe)"
    )
    
    parser.add_argument(
        "--image-base", 
        type=lambda x: int(x, 0),
        default=0x400000,
        help="Adresse de base de l'image (défaut: 0x400000)"
    )
    
    parser.add_argument(
        "--memory-base", 
        type=lambda x: int(x, 0),
        default=0x01CF4064,
        help="Adresse de base magique pour les offsets (défaut: 0x01CF4064)"
    )
    
    parser.add_argument(
        "--bytes-to-read", 
        type=int,
        default=15,
        help="Nombre d'octets à lire par instruction (défaut: 15)"
    )
    
    parser.add_argument(
        "--csv-delimiter", 
        default='\t',
        help="Délimiteur du fichier CSV (défaut: tabulation)"
    )
    
    parser.add_argument(
        "--log-file", 
        default="find_memory_usage.log",
        help="Fichier de log (défaut: find_memory_usage.log)"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Mode verbose (logs de debug)"
    )
    
    parser.add_argument(
        "--max-offset-range",
        type=lambda x: int(x, 0),
        default=0x10000,
        help="Plage maximale pour considérer un offset comme lié à memory_base (défaut: 0x10000)"
    )
    
    parser.add_argument(
        "--output",
        help="Fichier de sortie TOML (défaut: stdout)"
    )
    
    return parser.parse_args()

def find_memory_references_in_bytes(instruction_bytes, memory_base, max_range):
    """
    Trouve les références mémoire dans les bytes d'instruction et calcule les offsets à patcher
    
    Args:
        instruction_bytes (bytes): Bytes bruts de l'instruction
        memory_base (int): Adresse de base mémoire de référence
        max_range (int): Plage maximale pour considérer un offset comme valide
        
    Returns:
        list: Liste des offsets à patcher dans les bytes de l'instruction
    """
    patch_offsets = []
    
    # Chercher des adresses 32-bit dans les bytes (little endian)
    for i in range(len(instruction_bytes) - 3):
        # Extraire un dword (4 bytes) en little endian
        addr_bytes = instruction_bytes[i:i+4]
        addr = struct.unpack('<I', addr_bytes)[0]
        
        # Vérifier si cette adresse est dans la plage de memory_base
        if abs(addr - memory_base) <= max_range:
            offset_value = addr - memory_base
            patch_offsets.append({
                'byte_offset': i,
                'memory_offset': offset_value,
                'original_addr': addr
            })
    
    return patch_offsets

def create_patched_bytes_string(instruction_bytes, patch_offsets):
    """
    Crée une chaîne de bytes avec des 'X' pour marquer les bytes à patcher
    
    Args:
        instruction_bytes (bytes): Bytes originaux de l'instruction
        patch_offsets (list): Liste des offsets à patcher
        
    Returns:
        str: Chaîne hexadécimale avec 'X' pour les bytes à patcher
    """
    # Convertir les bytes en liste modifiable
    hex_chars = []
    for b in instruction_bytes:
        hex_chars.extend([f"{b:02X}"[0], f"{b:02X}"[1]])
    
    # Marquer les bytes à patcher avec 'X'
    for patch in patch_offsets:
        start_idx = patch['byte_offset'] * 2
        # Marquer 4 bytes (8 caractères hex) avec des X
        for j in range(8):
            if start_idx + j < len(hex_chars):
                hex_chars[start_idx + j] = 'X'
    
    # Regrouper par bytes (2 caractères)
    result = []
    for i in range(0, len(hex_chars), 2):
        if i + 1 < len(hex_chars):
            result.append(hex_chars[i] + hex_chars[i + 1])
        else:
            result.append(hex_chars[i] + '0')
    
    return ' '.join(result)

def load_binary(binary_file, logger):
    """Charge le fichier binaire"""
    logger.info(f"Chargement du fichier binaire: {binary_file}")
    try:
        with open(binary_file, "rb") as f:
            binary_data = f.read()
        logger.info(f"Fichier binaire chargé avec succès. Taille: {len(binary_data)} octets")
        return binary_data
    except FileNotFoundError:
        logger.error(f"Fichier binaire non trouvé: {binary_file}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Erreur lors du chargement du binaire: {e}")
        sys.exit(1)

def setup_capstone(logger):
    """Initialise Capstone"""
    logger.info("Initialisation de Capstone")
    try:
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        md.detail = False
        logger.info("Capstone initialisé avec succès")
        return md
    except Exception as e:
        logger.error(f"Erreur lors de l'initialisation de Capstone: {e}")
        sys.exit(1)

def read_csv_addresses(csv_file, delimiter, logger):
    """Lit les adresses depuis le fichier CSV"""
    logger.info(f"Lecture des adresses depuis le fichier CSV: {csv_file}")
    addresses = []
    invalid_addresses = 0
    
    try:
        with open(csv_file, newline='') as csvfile:
            reader = csv.reader(csvfile, delimiter=delimiter)
            for row_num, row in enumerate(reader, 1):
                if not row or not row[0].strip().startswith("0x"):
                    logger.debug(f"Ligne {row_num} ignorée (format invalide): {row}")
                    continue
                try:
                    addr = int(row[0], 16)
                    addresses.append(addr)
                    logger.debug(f"Adresse ajoutée: 0x{addr:08X}")
                except ValueError:
                    invalid_addresses += 1
                    logger.warning(f"Adresse invalide ligne {row_num}: {row[0]}")
                    continue
        
        logger.info(f"Lecture terminée. {len(addresses)} adresses valides trouvées")
        if invalid_addresses > 0:
            logger.warning(f"{invalid_addresses} adresses invalides ignorées")
        
        return addresses

    except FileNotFoundError:
        logger.error(f"Fichier CSV non trouvé: {csv_file}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Erreur lors de la lecture du CSV: {e}")
        sys.exit(1)

def main():
    # Parse des arguments
    args = parse_arguments()
    
    # Configuration du logging
    logger = setup_logging(args.log_file, args.verbose)
    
    logger.info("=== Démarrage du script de pré-patching ===")
    logger.info(f"Fichier CSV: {args.csv}")
    logger.info(f"Fichier binaire: {args.binary}")
    logger.info(f"Base d'image: 0x{args.image_base:08X}")
    logger.info(f"Base mémoire: 0x{args.memory_base:08X}")
    logger.info(f"Octets à lire par instruction: {args.bytes_to_read}")
    logger.info(f"Délimiteur CSV: {repr(args.csv_delimiter)}")
    logger.info(f"Plage d'offset max: 0x{args.max_offset_range:X}")
    
    # Chargement des composants
    binary_data = load_binary(args.binary, logger)
    md = setup_capstone(logger)
    addresses = read_csv_addresses(args.csv, args.csv_delimiter, logger)
    
    # Désassemblage et création du TOML
    logger.info("Début du désassemblage et pré-patching")
    instructions = {}
    successful_disassemblies = 0
    out_of_range_addresses = 0
    failed_disassemblies = 0

    for i, addr in enumerate(addresses):
        if i % 100 == 0 and i > 0:
            logger.info(f"Progression: {i}/{len(addresses)} adresses traitées")
        
        offset = addr - args.image_base
        if offset < 0 or offset + args.bytes_to_read > len(binary_data):
            out_of_range_addresses += 1
            logger.debug(f"Adresse hors limites: 0x{addr:08X} (offset: {offset})")
            continue  # Skip out-of-range
        
        code = binary_data[offset:offset + args.bytes_to_read]
        logger.debug(f"Désassemblage à 0x{addr:08X}, offset: {offset}")

        try:
            insns = list(md.disasm(code, addr, count=1))
            if not insns:
                failed_disassemblies += 1
                logger.debug(f"Échec du désassemblage à 0x{addr:08X}")
                continue
            
            insn = insns[0]
            
            # Obtenir les bytes de l'instruction (longueur réelle)
            instruction_bytes = code[:insn.size]
            
            # Trouver les références mémoire à patcher
            patch_offsets = find_memory_references_in_bytes(
                instruction_bytes, args.memory_base, args.max_offset_range
            )
            
            # Créer la chaîne de bytes avec les marqueurs X
            patched_bytes = create_patched_bytes_string(instruction_bytes, patch_offsets)
            
            # Calculer l'offset principal (premier trouvé ou 0)
            main_offset = patch_offsets[0]['memory_offset'] if patch_offsets else 0
            
            # Ajouter au dictionnaire des instructions
            addr_key = f"0x{insn.address:08X}"
            instructions[addr_key] = {
                "bytes": patched_bytes,
                "offset": f"0x{main_offset:X}" if main_offset >= 0 else f"-0x{abs(main_offset):X}"
            }
            
            successful_disassemblies += 1
            logger.debug(f"Pré-patching réussi: {addr_key} bytes={patched_bytes} offset={instructions[addr_key]['offset']}")
        
        except Exception as e:
            failed_disassemblies += 1
            logger.error(f"Erreur lors du désassemblage à 0x{addr:08X}: {e}")

    # Statistiques finales
    logger.info("=== Statistiques finales ===")
    logger.info(f"Total d'adresses à traiter: {len(addresses)}")
    logger.info(f"Pré-patchings réussis: {successful_disassemblies}")
    logger.info(f"Adresses hors limites: {out_of_range_addresses}")
    logger.info(f"Échecs de désassemblage: {failed_disassemblies}")
    logger.info(f"Taux de réussite: {(successful_disassemblies / len(addresses) * 100):.2f}%" if addresses else "N/A")
    logger.info("=== Fin du script ===")
    
    # Création du dictionnaire TOML final
    result = {
        "metadata": {
            "script_version": "1.0.0",
            "memory_base": f"0x{args.memory_base:08X}",
            "image_base": f"0x{args.image_base:08X}",
            "total_instructions": successful_disassemblies,
            "description": "Instructions pré-patchées pour ff8_hook"
        },
        "instructions": instructions
    }
    
    # Sortie TOML
    toml_content = toml.dumps(result)
    
    if args.output:
        # Écrire dans un fichier avec encodage UTF-8 explicite
        logger.info(f"Écriture du fichier TOML: {args.output}")
        try:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(toml_content)
            logger.info(f"Fichier TOML créé avec succès: {args.output}")
        except Exception as e:
            logger.error(f"Erreur lors de l'écriture du fichier TOML: {e}")
            sys.exit(1)
    else:
        # Sortie vers stdout
        print(toml_content)

if __name__ == "__main__":
    main() 