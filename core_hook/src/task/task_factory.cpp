#include "../../include/task/task_factory.hpp"
#include "../../include/util/logger.hpp"
#include <algorithm>

namespace app_hook::task {

TaskFactory& TaskFactory::instance() {
    static TaskFactory instance;
    LOG_DEBUG("TaskFactory::instance() returning singleton at address: 0x{:X}", reinterpret_cast<uintptr_t>(&instance));
    return instance;
}

bool TaskFactory::register_task_creator(const std::string& config_type_name, TaskCreatorFunc creator) {
    LOG_DEBUG("TaskFactory::register_task_creator called on instance at address: 0x{:X}", reinterpret_cast<uintptr_t>(this));
    
    if (config_type_name.empty() || !creator) {
        LOG_ERROR("Cannot register task creator: invalid config type name or creator function");
        return false;
    }

    if (creators_.find(config_type_name) != creators_.end()) {
        LOG_WARNING("Task creator for '{}' already registered - overwriting", config_type_name);
    }

    creators_[config_type_name] = std::move(creator);
    LOG_INFO("Registered task creator for config type: {}", config_type_name);
    LOG_DEBUG("TaskFactory now has {} total creators", creators_.size());
    return true;
}

HookTaskPtr TaskFactory::create_task(const config::ConfigBase& config) {
    const std::string config_type = typeid(config).name();
    
    LOG_DEBUG("TaskFactory::create_task called on instance at address: 0x{:X}", reinterpret_cast<uintptr_t>(this));
    LOG_DEBUG("TaskFactory::create_task called for config type: {}", config_type);
    LOG_DEBUG("TaskFactory has {} registered creators", creators_.size());
    
    // Log all registered creators for debugging
    if (creators_.empty()) {
        LOG_WARNING("TaskFactory has no registered creators!");
    } else {
        LOG_DEBUG("Registered creators:");
        for (const auto& [registered_type, _] : creators_) {
            LOG_DEBUG("  - {}", registered_type);
        }
    }
    
    // Try to find a creator by exact type name first
    auto it = creators_.find(config_type);
    if (it != creators_.end()) {
        LOG_DEBUG("Creating task using exact type match for: {}", config_type);
        return it->second(config);
    }
    LOG_DEBUG("No exact match found for: {}", config_type);

    // Try to find a creator by simplified type name (without namespace/decorations)
    for (const auto& [registered_type, creator] : creators_) {
        LOG_DEBUG("Checking partial match: '{}' contains '{}'?", config_type, registered_type);
        if (config_type.find(registered_type) != std::string::npos) {
            LOG_DEBUG("Creating task using partial type match '{}' for: {}", registered_type, config_type);
            auto task = creator(config);
            if (task) {
                return task;
            } else {
                LOG_WARNING("Task creator returned nullptr for type: {}", registered_type);
            }
        }
    }

    LOG_WARNING("No task creator found for config type: {} (key: {})", config_type, config.key());
    return nullptr;
}

bool TaskFactory::has_creator(const std::string& config_type_name) const {
    return creators_.find(config_type_name) != creators_.end();
}

std::vector<std::string> TaskFactory::get_registered_types() const {
    std::vector<std::string> types;
    types.reserve(creators_.size());
    
    for (const auto& [type_name, _] : creators_) {
        types.push_back(type_name);
    }
    
    std::sort(types.begin(), types.end());
    return types;
}

void TaskFactory::clear_creators() {
    LOG_INFO("Clearing all registered task creators");
    creators_.clear();
}

} // namespace app_hook::task 