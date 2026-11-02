/**
 * @file ups_index.h
 * @brief UPS workitems held by the index database, for the UPS services
 *
 * The database row holds the state, the lock and the keys the repository indexes. The whole
 * instance is a Part 10 file beside it, written first and replaced atomically, so a failed
 * update never leaves an acknowledged change half stored. Nothing here deletes a workitem,
 * so deletion locks hold without further work (PS3.4 CC.2.1.3).
 */

#ifndef PACS_SERVICES_UPS_INDEX_HPP
#define PACS_SERVICES_UPS_INDEX_HPP

#include "kcenon/pacs/services/event_sender.h"
#include "kcenon/pacs/services/ups/ups_scp.h"
#include "kcenon/pacs/storage/index_database.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace kcenon::pacs::services {

class ups_index {
public:
    /**
     * @param database The index; must outlive this object
     * @param dataset_directory Where the instances are kept, created when missing
     * @param events Sends reports to Receiving AEs; its resolver decides which AEs are known
     */
    ups_index(storage::index_database& database, std::filesystem::path dataset_directory,
              std::shared_ptr<event_sender> events, std::shared_ptr<di::ILogger> logger = nullptr);

    /// The one service a server registers, with its handlers bound to this index.
    [[nodiscard]] std::shared_ptr<ups_scp> make_service();

    /// CC.2.4.3: tell the known AEs that the SCP restarted. Call once the server listens.
    void report_restart();

    // The handlers, public so they can be tested without an association.
    [[nodiscard]] network::Result<std::monostate> create(const storage::ups_workitem& workitem);
    [[nodiscard]] network::Result<std::monostate> set(const std::string& uid, const core::dicom_dataset& changes);
    [[nodiscard]] network::Result<storage::ups_workitem> get(const std::string& uid);
    [[nodiscard]] network::Result<std::monostate> change_state(
        const std::string& uid, const std::string& state, const std::string& transaction);
    [[nodiscard]] network::Result<std::monostate> request_cancel(
        const std::string& uid, const core::dicom_dataset& request, const std::string& requesting_ae);
    [[nodiscard]] network::Result<std::monostate> subscribe(
        const std::string& receiving_ae, const std::string& uid, bool deletion_lock);
    [[nodiscard]] network::Result<std::monostate> unsubscribe(const std::string& receiving_ae, const std::string& uid);
    [[nodiscard]] network::Result<std::monostate> suspend(const std::string& receiving_ae);
    [[nodiscard]] std::vector<core::dicom_dataset> find(const core::dicom_dataset& keys);

private:
    [[nodiscard]] std::filesystem::path dataset_path(const std::string& uid) const;
    [[nodiscard]] bool write_dataset(const std::string& uid, const core::dicom_dataset& attributes) const;
    [[nodiscard]] core::dicom_dataset read_dataset(const std::string& uid) const;
    [[nodiscard]] std::vector<std::string> subscribers(const std::string& uid) const;
    [[nodiscard]] bool has_subscription(const std::string& receiving_ae, const std::string& uid) const;
    void notify(const std::vector<std::string>& receivers, const std::string& uid, uint16_t event_type,
                const core::dicom_dataset& information);
    void report_state(const std::vector<std::string>& receivers, const std::string& uid,
                      const std::string& state, const core::dicom_dataset& attributes);

    storage::index_database& database_;
    std::filesystem::path dataset_directory_;
    std::shared_ptr<event_sender> events_;
    std::shared_ptr<di::ILogger> logger_;
    std::mutex mutex_;
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_INDEX_HPP
