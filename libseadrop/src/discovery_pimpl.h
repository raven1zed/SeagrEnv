public:
DiscoveryState state = DiscoveryState::Uninitialized;
DiscoveryConfig config;
Device local_device;
bool is_receiving = false;
std::mutex mutex;

// Discovered devices
std::map<std::string, DiscoveredDevice> devices;

// Callbacks
std::function<void(const DiscoveredDevice &)> discovered_cb;
std::function<void(const DeviceId &)> lost_cb;
std::function<void(const DiscoveredDevice &)> updated_cb;
std::function<void(DiscoveryState)> state_changed_cb;
std::function<void(const Error &)> error_cb;

std::string device_key(const DeviceId &id) const { return id.to_hex(); }

void set_state(DiscoveryState new_state) {
  if (state != new_state) {
    state = new_state;
    if (state_changed_cb) {
      state_changed_cb(state);
    }
  }
}
}
;

// Platform hooks
Result<void> platform_discovery_start(DiscoveryManager::Impl *impl);
void platform_discovery_stop(DiscoveryManager::Impl *impl);
Result<void> platform_discovery_start_advertising(DiscoveryManager::Impl *impl);
void platform_discovery_stop_advertising(DiscoveryManager::Impl *impl);
Result<void> platform_discovery_start_scanning(DiscoveryManager::Impl *impl);
void platform_discovery_stop_scanning(DiscoveryManager::Impl *impl);

} // namespace seadrop

#endif // SEADROP_DISCOVERY_PIMPL_H
