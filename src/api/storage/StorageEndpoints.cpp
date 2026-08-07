#include "StorageEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "modules/storage/UserFilePolicy.h"

namespace StorageEndpoints {

Api::Response get(const FlashStorage &flashStorage,
                  const UserDataStorage &userData) {
  const FlashStorageSnapshot flash = flashStorage.snapshot();
  const UploadCapacity userCapacity = userData.uploadCapacity();
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
  user["partition_id"] = "userdata";
  user["filesystem"] = "littlefs";
  user["mounted"] = userData.mounted();

  JsonObject userCapabilities = user["capabilities"].to<JsonObject>();
  userCapabilities["file_upload"] = true;
  userCapabilities["file_list"] = true;
  userCapabilities["file_download"] = true;
  userCapabilities["file_delete"] = true;

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
