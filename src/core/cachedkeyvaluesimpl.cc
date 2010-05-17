/*
 * cachedkeyvaluesimpl.cc --
 *
 * Implementation of the CachedKeyValuesImpl class.
 *
 * ============================================================================
 * $Header:$
 * $Revision$
 * $Date$
 * ============================================================================
 */

#include "clusterlibinternal.h"

using namespace std;
using namespace json;

namespace clusterlib
{

CachedKeyValuesImpl::CachedKeyValuesImpl(NotifyableImpl *ntp)
    : CachedDataImpl(ntp) 
{
}

int32_t
CachedKeyValuesImpl::publish(bool unconditional)
{
    TRACE(CL_LOG, "publish");

    getNotifyable()->throwIfRemoved();

    string keyValuesKey = 
        PropertyListImpl::createKeyValJsonObjectKey(getNotifyable()->getKey());

    Locker l(&getCachedDataLock());

    string encodedJsonObject = JSONCodec::encode(m_keyValues);
    LOG_INFO(CL_LOG,
             "Tried to publish key values for notifyable %s to %s "
             "with current version %d, unconditional %d\n",
             getNotifyable()->getKey().c_str(), 
             encodedJsonObject.c_str(),
             getVersion(),
             unconditional);

    Stat stat;
    try {
        SAFE_CALL_ZK(getOps()->getRepository()->setNodeData(
                         keyValuesKey,
                         encodedJsonObject,
                         ((unconditional == false) ? getVersion(): -1),
                         &stat),
                     "Setting of %s failed: %s",
                     keyValuesKey.c_str(),
                     false,
                     true);
    } catch (const zk::BadVersionException &e) {
        throw PublishVersionException(e.what());
    }
    
    /* 
     * Since we should have the lock, the data should be identical to
     * the zk data.  When the lock is released, clusterlib events will
     * try to push this change again.  
     */
    setStat(stat);
    return stat.version;
}

void
CachedKeyValuesImpl::loadDataFromRepository(bool setWatchesOnly)
{
    TRACE(CL_LOG, "loadDataFromRepository");
    
    string keyValuesKey = 
        PropertyListImpl::createKeyValJsonObjectKey(getNotifyable()->getKey());
    string encodedJsonValue;
    Stat stat;

    Locker l(&getCachedDataLock());

    SAFE_CALLBACK_ZK(
        getOps()->getRepository()->getNodeData(
            keyValuesKey,
            encodedJsonValue,
            getOps()->getZooKeeperEventAdapter(),
            getOps()->getCachedObjectChangeHandlers()->
            getChangeHandler(
                CachedObjectChangeHandlers::PROPERTYLIST_VALUES_CHANGE),
            &stat),
        getOps()->getRepository()->getNodeData(
            keyValuesKey, encodedJsonValue, NULL, NULL, &stat),
        CachedObjectChangeHandlers::PROPERTYLIST_VALUES_CHANGE,
        keyValuesKey,
        "Loading keyValuesKey %s failed: %s",
        keyValuesKey.c_str(),
        false,
        true);

    if (setWatchesOnly) {
        return;
    }

    if (!updateStat(stat)) {
        return;
    }
    
    /* 
     * Default values from the constructor are used when there are
     * empty nodes 
     */
    if (encodedJsonValue.empty()) {
        return;
    }

    m_keyValues = 
        JSONCodec::decode(encodedJsonValue).get<JSONValue::JSONObject>();
}

vector<JSONValue::JSONString>
CachedKeyValuesImpl::getKeys()
{
    Locker l(&getCachedDataLock());

    vector<JSONValue::JSONString> keys;
    JSONValue::JSONObject::const_iterator keyValuesIt;
    for (keyValuesIt = m_keyValues.begin(); 
         keyValuesIt != m_keyValues.end(); 
         ++keyValuesIt) {
        keys.push_back(keyValuesIt->first);
    }

    return keys;
}

bool
CachedKeyValuesImpl::get(
    const JSONValue::JSONString &key, 
    JSONValue &jsonValue,
    bool searchParent, 
    PropertyList **propertyListWithKey)
{
    TRACE(CL_LOG, "get");
    
    getNotifyable()->throwIfRemoved();

    Locker l(&getCachedDataLock());
    JSONValue::JSONObject::const_iterator ssIt = m_keyValues.find(key);
    if (ssIt != m_keyValues.end()) {
        LOG_DEBUG(CL_LOG,
                  "get: Found key (%s) with val (%s) "
                  "in PropertyList key (%s), version (%d)",
                  key.c_str(),
                  JSONCodec::encode(ssIt->second).c_str(),
                  getNotifyable()->getKey().c_str(),
                  getVersion());
        jsonValue = ssIt->second;
        if (propertyListWithKey != NULL) {
            *propertyListWithKey = 
                dynamic_cast<PropertyList *>(getNotifyable());
        }
        return true;
    }
    else if (searchParent == false) {
        LOG_DEBUG(CL_LOG,
                  "get: Did not find key (%s) "
                  "in PropertyList key (%s), version (%d)",
                  key.c_str(),
                  getNotifyable()->getKey().c_str(),
                  getVersion());
        /*
         * Don't try the parent if not explicit
         */
        return false;
    }

    /*
     * Key manipulation should only be done in clusterlib.cc, therefore
     * this should move to clusterlib.cc.
     */
    PropertyList *prop = NULL;
    string parentKey = getNotifyable()->getKey();
    do {
        /*
         * Generate the new parentKey by removing this PropertyList
         * object and one clusterlib object.
         */
        parentKey = NotifyableKeyManipulator::removeObjectFromKey(parentKey);
        parentKey = NotifyableKeyManipulator::removeObjectFromKey(parentKey);

        if (parentKey.empty()) {
            LOG_DEBUG(CL_LOG,
                      "getProperty: Giving up with new key %s from old key %s",
                      parentKey.c_str(),
                      getNotifyable()->getKey().c_str());
            return false;
        }
        parentKey.append(ClusterlibStrings::KEYSEPARATOR);
        parentKey.append(ClusterlibStrings::PROPERTYLISTS);
        parentKey.append(ClusterlibStrings::KEYSEPARATOR);
        parentKey.append(getNotifyable()->getName());

        LOG_DEBUG(CL_LOG,
                  "getProperty: Trying new key %s from old key %s",
                  parentKey.c_str(),
                  getNotifyable()->getKey().c_str());
        
        prop = dynamic_cast<PropertyList *>(
            getOps()->getNotifyableFromKey(
                vector<string>(
                    1, 
                    ClusterlibStrings::REGISTERED_PROPERTYLIST_NAME), 
                parentKey));
    } while (prop == NULL);
    
    return prop->cachedKeyValues().get(
        key, jsonValue, searchParent, propertyListWithKey);
}

void
CachedKeyValuesImpl::set(
    const JSONValue::JSONString &key, const JSONValue &jsonValue)
{
    Locker l(&getCachedDataLock());

    m_keyValues[key] = jsonValue;
}

bool
CachedKeyValuesImpl::erase(const JSONValue::JSONString &key)
{
    Locker l(&getCachedDataLock());
    
    return (m_keyValues.erase(key) == 1) ? true : false;
}

void
CachedKeyValuesImpl::clear()
{
    Locker l(&getCachedDataLock());
    
    m_keyValues.clear();
}

};	/* End of 'namespace clusterlib' */