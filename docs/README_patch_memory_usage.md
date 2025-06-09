# patch_memory_usage.py - Documentation

## Vue d'ensemble

`patch_memory_usage.py` est un script de pré-patching d'instructions pour le projet **ff8_hook**. Il transforme les instructions assembleur extraites d'IDA Pro en versions pré-patchées qui permettent une relocation dynamique de la mémoire.

## Objectif

Le script prend en entrée :
- Un fichier CSV d'adresses mémoire extraites d'IDA Pro
- Un fichier binaire (ex: `FF8_EN.exe`)

Et génère en sortie :
- Un fichier TOML contenant les instructions pré-patchées où les adresses absolues sont remplacées par des références relatives à `<memory_base>`

## Pipeline de traitement

```
CSV d'IDA → Désassemblage → Pré-patching → TOML → ff8_hook
```

1. **Extraction CSV** : IDA Pro exporte les adresses d'usage mémoire
2. **Désassemblage** : Capstone désassemble les instructions à ces adresses
3. **Pré-patching** : Les adresses absolues sont remplacées par `<memory_base>+offset`
4. **Export TOML** : Format structuré pour ff8_hook
5. **Relocation** : ff8_hook remplace `<memory_base>` par la nouvelle base mémoire

## Installation

### Prérequis

```bash
pip install capstone toml
```

### Dépendances Python

- `capstone` : Désassembleur
- `toml` : Format de sortie
- `csv`, `argparse`, `logging` : Modules standard

## Usage

### Syntaxe de base

```bash
python patch_memory_usage.py --csv <fichier_csv> --binary <fichier_binaire>
```

### Exemple complet

```bash
python patch_memory_usage.py \
    --csv ida_memory_usage.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    --image-base 0x400000 \
    --verbose
```

### Options disponibles

| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `--csv` | **Requis** | - | Fichier CSV avec les adresses IDA |
| `--binary` | **Requis** | - | Fichier binaire à analyser |
| `--memory-base` | Hex | `0x01CF4064` | Base mémoire pour les offsets |
| `--image-base` | Hex | `0x400000` | Base d'image du binaire |
| `--bytes-to-read` | Int | `15` | Octets par instruction |
| `--csv-delimiter` | Char | `\t` | Délimiteur CSV |
| `--max-offset-range` | Hex | `0x10000` | Plage d'offset maximum |
| `--log-file` | String | `find_memory_usage.log` | Fichier de log |
| `--verbose`, `-v` | Flag | `false` | Mode debug |

## Format des fichiers

### Fichier CSV d'entrée

Format attendu depuis IDA Pro :
```csv
0x0048D774
0x0048D8A4
0x00496C11
0x00496AD9
```

Chaque ligne contient une adresse hexadécimale où une instruction accède à la zone mémoire cible.

### Fichier TOML de sortie

```toml
[metadata]
script_version = "1.0.0"
memory_base = "0x01CF4064"
image_base = "0x400000"
total_instructions = 142
description = "Instructions pré-patchées pour ff8_hook"

[instructions]
"0x0048D774" = "lea eax, [esi + <memory_base>+0x2A]"
"0x0048D8A4" = "lea edx, [eax + <memory_base>+0x2A]"
"0x00496C11" = "mov bp, word ptr [eax + <memory_base>+0x28]"
"0x00496AD9" = "mov al, byte ptr [eax*4 + <memory_base>+0x26]"
```

## Intégration avec ff8_hook

### 1. Génération du TOML

```bash
# Générer le fichier de patches
python patch_memory_usage.py \
    --csv ida_magic_usage.csv \
    --binary FF8_EN.exe \
    > magic_patches.toml
```

### 2. Utilisation dans ff8_hook

Le projet ff8_hook peut ensuite :
1. Charger le fichier TOML
2. Allocuer une nouvelle zone mémoire
3. Remplacer toutes les occurrences de `<memory_base>` par la nouvelle adresse
4. Appliquer les patches au runtime

### 3. Exemple d'utilisation ff8_hook

```cpp
// Pseudo-code ff8_hook
void* new_memory = allocate_memory(size);
uintptr_t new_base = (uintptr_t)new_memory;

// Remplacer <memory_base> par new_base dans toutes les instructions
for (auto& [addr, instruction] : patches.instructions) {
    std::string patched = replace_memory_base(instruction, new_base);
    apply_patch(addr, patched);
}
```

## Workflow complet

### 1. Extraction depuis IDA Pro

1. Ouvrir `FF8_EN.exe` dans IDA Pro
2. Identifier la zone mémoire cible (ex: magic data à `0x01CF4064`)
3. Chercher toutes les références (Ctrl+X sur l'adresse)
4. Exporter les adresses au format CSV

### 2. Pré-patching

```bash
python patch_memory_usage.py \
    --csv ida_refs.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    --verbose \
    > magic_patches.toml
```

### 3. Intégration ff8_hook

1. Charger `magic_patches.toml` dans ff8_hook
2. Allocuer nouvelle mémoire
3. Copier les données originales
4. Appliquer les patches avec la nouvelle base
5. Rediriger l'accès mémoire

## Logging et débogage

### Niveaux de log

- **INFO** : Progression générale, statistiques
- **DEBUG** : Détails de chaque instruction (avec `--verbose`)
- **WARNING** : Adresses invalides ignorées
- **ERROR** : Erreurs de traitement

### Fichier de log

Le fichier `find_memory_usage.log` contient :
```
2024-01-15 14:30:15 - INFO - === Démarrage du script de pré-patching ===
2024-01-15 14:30:15 - INFO - Fichier CSV: ida_usage.csv
2024-01-15 14:30:15 - INFO - Base mémoire: 0x01CF4064
2024-01-15 14:30:16 - INFO - Pré-patchings réussis: 142
2024-01-15 14:30:16 - INFO - Taux de réussite: 98.61%
```

## Troubleshooting

### Erreurs communes

1. **"Fichier binaire non trouvé"**
   - Vérifier le chemin vers `FF8_EN.exe`
   - S'assurer que le fichier est accessible

2. **"Adresses invalides"**
   - Vérifier le format CSV (adresses commençant par `0x`)
   - Contrôler le délimiteur (tabulation par défaut)

3. **"Échecs de désassemblage"**
   - Ajuster `--bytes-to-read` (15 par défaut)
   - Vérifier `--image-base` (0x400000 par défaut)

### Mode debug

```bash
python patch_memory_usage.py --csv data.csv --binary game.exe --verbose
```

Le mode verbose affiche chaque instruction traitée et facilite le débogage.

## Exemples avancés

### Traitement de multiples zones mémoire

```bash
# Zone magic
python patch_memory_usage.py \
    --csv magic_refs.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    > magic_patches.toml

# Zone items  
python patch_memory_usage.py \
    --csv items_refs.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF8000 \
    > items_patches.toml
```

### Configuration personnalisée

```bash
# Binaire avec base différente
python patch_memory_usage.py \
    --csv refs.csv \
    --binary custom_game.exe \
    --image-base 0x10000000 \
    --memory-base 0x20000000 \
    --max-offset-range 0x20000 \
    > custom_patches.toml
```

## Licence et contribution

Ce script fait partie du projet ff8_hook. Consultez le README principal pour les informations de licence et de contribution. 