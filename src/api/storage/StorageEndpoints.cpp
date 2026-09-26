#include "StorageEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "modules/storage/UserFilePolicy.h"

namespace StorageEndpoints {

Api::Response get(const FlashStorage &flashStorage
#if IOT_FEATURE_STORAGE
                  , const UserDataStorage &userData
#endif
) {
  const FlashStorageSnapshot flash = flashStorage.snapshot();
#if IOT_FEATURE_STORAGE
  const UploadCapacity userCapacity = userData.uploadCapacity();
#else
  const UploadCapacity userCapacity{};
#endif
  JsonDocument data;

  JsonObject flashObject = data["flash"].to<JsonObject>();
  flashObject["total_bytes"] = flash.totalBytes;
  JsonObject fixedRegions = flashObject["fixed_regions"].to<JsonObject>();
  fixedRegions["bootloader_reserved_bytes"] = flash.bootloaderReservedBytes;
  fixedRegions["partition_table_bytes"] = flash.partitionTableBytes;
  JsonArray partitions = flashObject["partitions"].to<JsonArray>();
  for (size_t i = 0; i < flash.partitionCount; ++i) {
    const FlashPartitionCapacity &partition = flash.partitions[i];
    JsonObject item = partitions.add<JsonObject>();
    item["id"] = partition.id;
    item["type"] = partition.type;
    item["subtype"] = partition.subtype;
    item["offset_bytes"] = partition.offsetBytes;
    item["size_bytes"] = partition.sizeBytes;
  }

  JsonObject app = data["app"].to<JsonObject>();
  app["partition_id"] = flash.app.partitionId;
  app["frontend_bundled"] = flash.app.frontendBundled;
  JsonObject appCapacity = app["capacity"].to<JsonObject>();
  appCapacity["total_bytes"] = flash.app.totalBytes;
  appCapacity["firmware_image_bytes"] = flash.app.imageBytes;
  appCapacity["frontend_payload_bytes"] = flash.app.frontendBytes;
  appCapacity["available_bytes"] = flash.app.availableBytes;

  JsonObject user = data["user"].to<JsonObject>();
#if IOT_FEATURE_STORAGE
  user["partition_id"] = "userdata";
  user["filesystem"] = "littlefs";
  user["mounted"] = userData.mounted();
#else
  user["partition_id"] = nullptr;
  user["filesystem"] = nullptr;
  user["mounted"] = false;
#endif

  JsonObject userCapabilities = user["capabilities"].to<JsonObject>();
  userCapabilities["file_upload"] = IOT_FEATURE_USER_FILES != 0;
  userCapabilities["file_list"] = IOT_FEATURE_USER_FILES != 0;
  userCapabilities["file_download"] = IOT_FEATURE_USER_FILES != 0;
  userCapabilities["file_delete"] = IOT_FEATURE_USER_FILES != 0;

  JsonObject uploadCapacity = user["capacity"].to<JsonObject>();
  uploadCapacity["total_bytes"] = userCapacity.totalBytes;
  uploadCapacity["used_bytes"] = userCapacity.usedBytes;
  uploadCapacity["available_bytes"] = userCapacity.availableBytes;

  JsonObject limits = user["limits"].to<JsonObject>();
  limits["max_upload_bytes"] = userCapacity.maxUploadBytes;
  limits["reserved_bytes"] = userCapacity.reservedBytes;
  limits["allocation_unit_bytes"] = userCapacity.allocationUnitBytes;
  limits["max_filename_bytes"] = UserFilePolicy::kMaxFilenameBytes;
  return Api::ok(Api::json(data));
}

}  // namespace StorageEndpoints
