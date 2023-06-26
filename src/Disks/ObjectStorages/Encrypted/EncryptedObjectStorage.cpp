#include "EncryptedObjectStorage.h"

#include <filesystem>
#include <Disks/ObjectStorages/DiskObjectStorageCommon.h>
#include <IO/BoundedReadBuffer.h>
#include <IO/ReadBufferFromFileBase.h>
#include <IO/WriteBufferFromEncryptedFile.h>
#include <Interpreters/Context.h>
#include <Common/CurrentThread.h>
#include <Common/logger_useful.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
    extern const int DATA_ENCRYPTION_ERROR;
}

String EncryptedObjectStorageSettings::findKeyByFingerprint(UInt128 key_fingerprint, const String & path_for_logs) const
{
    auto it = all_keys.find(key_fingerprint);
    if (it == all_keys.end())
    {
        throw Exception(
            ErrorCodes::DATA_ENCRYPTION_ERROR, "Not found an encryption key required to decipher file {}", quoteString(path_for_logs));
    }
    return it->second;
}

EncryptedObjectStorage::EncryptedObjectStorage(
    ObjectStoragePtr object_storage_, EncryptedObjectStorageSettingsPtr enc_settings_, const std::string & enc_config_name_)
    : object_storage(object_storage_), enc_settings(enc_settings_), enc_config_name(enc_config_name_), log(&Poco::Logger::get(getName()))
{
}

DataSourceDescription EncryptedObjectStorage::getDataSourceDescription() const
{
    auto wrapped_object_storage_data_source = object_storage->getDataSourceDescription();
    wrapped_object_storage_data_source.is_encrypted = true;
    return wrapped_object_storage_data_source;
}

std::string EncryptedObjectStorage::generateBlobNameForPath(const std::string & path)
{
    return object_storage->generateBlobNameForPath(path);
}

ReadSettings EncryptedObjectStorage::patchSettings(const ReadSettings & read_settings) const
{
    ReadSettings modified_settings{read_settings};
    modified_settings.encryption_settings = enc_settings;
    return object_storage->patchSettings(modified_settings);
}

void EncryptedObjectStorage::startup()
{
    object_storage->startup();
}

bool EncryptedObjectStorage::exists(const StoredObject & object) const
{
    return object_storage->exists(object);
}

std::unique_ptr<ReadBufferFromFileBase> EncryptedObjectStorage::readObjects( /// NOLINT
    const StoredObjects & objects,
    const ReadSettings & read_settings,
    std::optional<size_t> read_hint,
    std::optional<size_t> file_size) const
{
    if (objects.empty())
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Received empty list of objects to read");
    return object_storage->readObjects(objects, patchSettings(read_settings), read_hint, file_size);
}

std::unique_ptr<ReadBufferFromFileBase> EncryptedObjectStorage::readObject( /// NOLINT
    const StoredObject & object,
    const ReadSettings & read_settings,
    std::optional<size_t> read_hint,
    std::optional<size_t> file_size) const
{
    return readObjects({object}, patchSettings(read_settings), read_hint, file_size);
}

std::unique_ptr<WriteBufferFromFileBase> EncryptedObjectStorage::writeObject( /// NOLINT
    const StoredObject & object,
    WriteMode mode, // EncryptedObjectStorage doesn't support append, only rewrite
    std::optional<ObjectAttributes> attributes,
    size_t buf_size,
    const WriteSettings & write_settings)
{
    auto modified_write_settings = IObjectStorage::patchSettings(write_settings);
    auto implementation_buffer = object_storage->writeObject(object, mode, attributes, buf_size, modified_write_settings);
    FileEncryption::Header header;
    header.algorithm = enc_settings->current_algorithm;
    header.key_fingerprint = enc_settings->current_key_fingerprint;
    header.init_vector = FileEncryption::InitVector::random();
    return std::make_unique<WriteBufferFromEncryptedFile>(buf_size, std::move(implementation_buffer), enc_settings->current_key, header, 0);
}

void EncryptedObjectStorage::removeObject(const StoredObject & object)
{
    object_storage->removeObject(object);
}

void EncryptedObjectStorage::removeObjects(const StoredObjects & objects)
{
    object_storage->removeObjects(objects);
}

void EncryptedObjectStorage::removeObjectIfExists(const StoredObject & object)
{
    object_storage->removeObjectIfExists(object);
}

void EncryptedObjectStorage::removeObjectsIfExist(const StoredObjects & objects)
{
    object_storage->removeObjectsIfExist(objects);
}

void EncryptedObjectStorage::copyObject( // NOLINT
    const StoredObject & object_from,
    const StoredObject & object_to,
    std::optional<ObjectAttributes> object_to_attributes)
{
    object_storage->copyObject(object_from, object_to, object_to_attributes);
}

std::unique_ptr<IObjectStorage> EncryptedObjectStorage::cloneObjectStorage(
    const std::string & new_namespace,
    const Poco::Util::AbstractConfiguration & config,
    const std::string & config_prefix,
    ContextPtr context)
{
    return object_storage->cloneObjectStorage(new_namespace, config, config_prefix, context);
}

void EncryptedObjectStorage::listObjects(const std::string & path, RelativePathsWithMetadata & children, int max_keys) const
{
    object_storage->listObjects(path, children, max_keys);
}

ObjectMetadata EncryptedObjectStorage::getObjectMetadata(const std::string & path) const
{
    return object_storage->getObjectMetadata(path);
}

void EncryptedObjectStorage::shutdown()
{
    object_storage->shutdown();
}

void EncryptedObjectStorage::applyNewSettings(
    const Poco::Util::AbstractConfiguration & config, const std::string & config_prefix, ContextPtr context)
{
    object_storage->applyNewSettings(config, config_prefix, context);
}

String EncryptedObjectStorage::getObjectsNamespace() const
{
    return object_storage->getObjectsNamespace();
}

}
