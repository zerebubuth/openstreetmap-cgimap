#include "cgimap/options.hpp"
#include "cgimap/logger.hpp"


global_settings_base::~global_settings_base() = default;

std::unique_ptr<global_settings_base> global_settings::settings = std::unique_ptr<global_settings_base>(new global_settings_default());
