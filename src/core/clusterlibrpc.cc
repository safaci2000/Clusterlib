/*
 * clusterlibrpc.cc --
 *
 * Implementation of the ClusterlibRPCRequest class.
 *
 * ============================================================================
 * $Header:$
 * $Revision$
 * $Date$
 * ============================================================================
 */

#include "clusterlibinternal.h"
#include "clusterlibrpc.h"

#define LOG_LEVEL LOG_WARN
#define MODULE_NAME "ClusterLib"

using namespace std;
using namespace json;
using namespace json::rpc;

namespace clusterlib
{

ClusterlibRPCRequest::ClusterlibRPCRequest(Client *client, 
                                           ClientData data) 
    : m_gotResponse(false),
      m_data(data)
{
    TRACE(CL_LOG, "ClusterlibRPCRequest");

    m_client = dynamic_cast<ClientImpl *>(client);
    m_root = m_client->getRoot();
}

void
ClusterlibRPCRequest::sendRequest(const void *destination)
{
    TRACE(CL_LOG, "sendRequest");

    if (destination == NULL) {
        throw InvalidArgumentsException("sendRequest: Destination is NULL");
    }
    const char *queuePtr = reinterpret_cast<const char *>(destination);
    
    Queue *queue = 
        dynamic_cast<Queue *>(m_root->getNotifyableFromKey(queuePtr));
    if (queue == NULL) {
        throw InvalidArgumentsException(
            string("sendRequest: Invalid queue at key ") + queuePtr);
    }
    else {
        /* Ready the response */
        stringstream idSs;
        idSs << ProcessThreadService::getHostnamePidTid() 
             << m_client->fetchAndIncrRequestCounter();
        m_id = idSs.str();
        m_client->getOps()->getResponseSignalMap()->addRefPredMutexCond(m_id);

        /*
         * Marshal and check the parameters according to user-defined
         * functions.
         */
        m_paramArr = marshalParams();
        checkParams(m_paramArr);

        /* Set the response queue if not empty */
        if (!getRespQueueKey().empty()) {
            if (m_paramArr.size() == 0) {
                JSONValue::JSONObject jsonObj;
                jsonObj[ClusterlibStrings::JSONOBJECTKEY_RESPQUEUEKEY] = 
                    getRespQueueKey();
                m_paramArr.push_back(jsonObj);
            }
            else {
                JSONValue::JSONObject jsonObj = 
                    m_paramArr[0].get<JSONValue::JSONObject>();
                jsonObj[ClusterlibStrings::JSONOBJECTKEY_RESPQUEUEKEY] = 
                    getRespQueueKey();
                m_paramArr[0] = jsonObj;
            }
        }

        /* Format according the JSON-RPC 1.0 */
        JSONValue::JSONObject rpcObj;
        rpcObj["method"] = getName();
        rpcObj["params"] = m_paramArr;
        rpcObj["id"] = idSs.str();
        LOG_DEBUG(CL_LOG, 
                  "sendRequest: Putting request (%s) on queue (%s) "
                  "with id (%s)",
                  JSONCodec::encode(rpcObj).c_str(),
                  queue->getKey().c_str(),
                  m_id.c_str());
        queue->put(JSONCodec::encode(rpcObj));
    }
}

void
ClusterlibRPCRequest::waitResponse()
{
    TRACE(CL_LOG, "waitResponse");

    if (!waitMsecsResponse(-1)) {
        throw InconsistentInternalStateException(
            "waitResponse: waitMsecsResponse impossibly returned false!");
    }
}

bool
ClusterlibRPCRequest::waitMsecsResponse(int64_t msecsTimeout)
{
    TRACE(CL_LOG, "waitMsecsResponse");

    if (!m_gotResponse) {
        int64_t usecsTimeout = (msecsTimeout == -1) ? -1 : msecsTimeout * 1000;
        m_gotResponse =
            m_client->getOps()->getResponseSignalMap()->waitUsecsPredMutexCond(
                m_id, usecsTimeout);
        if (!m_gotResponse) {
            return false;
        }

        m_client->getOps()->getResponseSignalMap()->removeRefPredMutexCond(
            m_id);
        m_response = m_client->getOps()->getIdResponse(m_id);
        return true;
    }

    /* Should never get here. */
    return false;
}

const JSONValue &
ClusterlibRPCRequest::getResponseResult() const
{
    TRACE(CL_LOG, "getResponseResult");

    if (!m_gotResponse) {
        throw InvalidMethodException(
            string("getResponseResult: Response not "
                   "received yet for ") + getName());
    }

    JSONValue::JSONObject::const_iterator resultIt = m_response.find("result");
    if (resultIt == m_response.end()) {
        throw InconsistentInternalStateException(
            "getResponseResult: Failed to find 'result' in response");
    }

    return resultIt->second;
}

const JSONValue &
ClusterlibRPCRequest::getResponseError() const
{
    TRACE(CL_LOG, "getResponseError");

    if (!m_gotResponse) {
        throw InvalidMethodException(
            string("getResponseError: Response not "
                   "received yet for ") + getName());
    }

    JSONValue::JSONObject::const_iterator errorIt = m_response.find("error");
    if (errorIt == m_response.end()) {
        throw InconsistentInternalStateException(
            "getResponseError: Failed to find 'error' in response");
    }

    return errorIt->second;
}

const JSONValue &
ClusterlibRPCRequest::getResponseId() const
{
    TRACE(CL_LOG, "getResponseId");

    if (!m_gotResponse) {
        throw InvalidMethodException(
            string("getResponseId: Response not "
                   "received yet for ") + getName());
    }

    JSONValue::JSONObject::const_iterator idIt = m_response.find("id");
    if (idIt == m_response.end()) {
        throw InconsistentInternalStateException(
            "getResponseId: Failed to find 'id' in response");
    }

    return idIt->second;
}

const JSONValue::JSONObject &
ClusterlibRPCRequest::getResponse() const
{
    TRACE(CL_LOG, "getResponse");

    if (!m_gotResponse) {
        throw InvalidMethodException(
            string("getResponse: Response not "
                   "received yet for ") + getName());
    }

    return m_response;
}

ClientData
ClusterlibRPCRequest::getClientData()
{
    TRACE(CL_LOG, "getClientData");

    return m_data;
}

void
ClusterlibRPCRequest::setClientData(ClientData data)
{
    TRACE(CL_LOG, "setClientData");
    
    m_data = data;
}

bool
ClusterlibRPCRequest::isValidJSONRPCRequest(
    const JSONValue::JSONObject &rpcObj)
{
    TRACE(CL_LOG, "isValidJSONRPCRequest");

    JSONValue::JSONObject::const_iterator rpcObjIt = rpcObj.find("method");
    if (rpcObjIt == rpcObj.end()) {
        LOG_WARN(CL_LOG, "isValidJSONRPCRequest: Couldn't find method");
        return false;
    }
    rpcObjIt = rpcObj.find("params");
    if (rpcObjIt == rpcObj.end()) {
        LOG_WARN(CL_LOG, "isValidJSONRPCRequest: Couldn't find params");
        return false;
    }
    rpcObjIt = rpcObj.find("id");
    if (rpcObjIt == rpcObj.end()) {
        LOG_WARN(CL_LOG, "isValidJSONRPCRequest: Couldn't find id");
        return false;
    }
    if (rpcObj.size() != 3) {
        LOG_WARN(
            CL_LOG,
            "isValidJSONRPCRequest: Should be exactly 3 keys and found %d",
            rpcObj.size());
        return false;
    }

    return true;
}

ClusterlibRPCManager::ClusterlibRPCManager(
    Root *root,
    Queue *recvQueue,
    Queue *completedQueue,
    int32_t completedQueueMaxSize,
    PropertyList *rPCMethodHandlerPropertylist)
    : m_root(root),
      m_recvQueue(recvQueue),
      m_completedQueue(completedQueue),
      m_completedQueueMaxSize(completedQueueMaxSize),
      m_RPCMethodHandlerPropertyList(rPCMethodHandlerPropertylist) 
{
    if (m_root == NULL) {
        throw InvalidArgumentsException(
            "ClusterlibRPCManager: No valid root");
    }
    if (m_recvQueue == NULL) {
        throw InvalidArgumentsException(
            "ClusterlibRPCManager: No valid recv queue");
    }
    if (m_completedQueue == NULL) {
        throw InvalidArgumentsException(
            "ClusterlibRPCManager: No valid completed queue");
    }    
    if (m_completedQueueMaxSize < -1) {
        throw InvalidArgumentsException(
            "ClusterlibRPCManager: No valid completed queue max size");
    }
}

bool
ClusterlibRPCMethod::setMethodStatus(const string &status,
                                     int32_t maxRetries,
                                     int32_t maxStatusesShown)
{
    TRACE(CL_LOG, "setMethodStatus");

    PropertyList *propertyList = 
        getRPCManager()->getRPCMethodHandlerPropertyList();
    if (propertyList == NULL) {
        return false;
    }

    int32_t retries = 0;
    bool gotLock = false;
    string statusKey = 
        ProcessThreadService::getHostnamePidTid() + " " + 
        ClusterlibStrings::PLK_RPCMANAGER_REQ_STATUS_POSTFIX;
    JSONValue::JSONInteger time;
    JSONValue::JSONString timeString;
    JSONValue::JSONArray allStatusArr;
    JSONValue::JSONArray lastStatusArr;
    JSONValue::JSONValue jsonValue;
    string encodedJsonArr;
    while ((maxRetries == -1) || (retries <= maxRetries)) {
        gotLock = propertyList->acquireLockWaitMsecs(100);
        if (gotLock) {
            allStatusArr.clear();
            lastStatusArr.clear();
            encodedJsonArr = propertyList->getProperty(statusKey);
            if (!encodedJsonArr.empty()) {
                jsonValue = JSONCodec::decode(encodedJsonArr);
                allStatusArr = jsonValue.get<JSONValue::JSONArray>();
            }
            time = TimerService::getCurrentTimeMsecs();
            timeString = TimerService::getMsecsTimeString(time);
            lastStatusArr.push_back(status);
            lastStatusArr.push_back(time);
            lastStatusArr.push_back(timeString);
            allStatusArr.push_back(lastStatusArr);
            /* Trim to the appropriate number of statuses */
            while ((maxStatusesShown != -1) &&
                   (maxStatusesShown < 
                    static_cast<int32_t>(allStatusArr.size()))) {
                allStatusArr.pop_front();
            }
            propertyList->setProperty(statusKey, 
                                      JSONCodec::encode(allStatusArr));
            try {
                propertyList->publish();
                propertyList->releaseLock();
                return true;
            }
            catch (const PublishVersionException &ex) {
                LOG_WARN(CL_LOG, 
                         "setMethodStatus: Failed to update status '%s' on "
                         "try %d (PublishVersionException)",
                         statusKey.c_str(),
                         retries);
            }
            propertyList->releaseLock();
        }
        ++retries;
    }

    LOG_DEBUG(CL_LOG, 
              "setMethodStatus: %s %s", 
              status.c_str(),
              JSONCodec::encode(allStatusArr).c_str());

    return false;
}

void
ClusterlibRPCMethod::setRPCManager(ClusterlibRPCManager *rPCManager)
{
    TRACE(CL_LOG, "setRPCManager");

    if (rPCManager == NULL) {
        throw InvalidArgumentsException("setRPCManager: NULL rPCManager");
    }

    m_RPCManager = rPCManager;
}

ClusterlibRPCManager *
ClusterlibRPCMethod::getRPCManager()
{
    TRACE(CL_LOG, "getRPCManager");

    if (m_RPCManager == NULL) {
        throw InvalidMethodException(
            "getRPCManager: Called prior to registerMethod()");
    }

    return m_RPCManager;
}

void
ClusterlibRPCManager::invokeAndResp(const string &rpcInvocation,
                                    StatePersistence *persistence)
{
    TRACE(CL_LOG, "invokeAndResp");
    
    JSONValue jsonInput, jsonResult;
    JSONValue::JSONArray jsonResultArr;
    JSONValue::JSONObject inputObj;
    JSONValue::JSONObject::const_iterator jsonInputIt;
    string encodedResult;
    string encodedResultArr;
    int64_t msecs;
    try {
        jsonInput = JSONCodec::decode(rpcInvocation);
        setBasicRequestStatus(jsonInput, true);
        jsonResult = invoke(jsonInput, persistence);
        setBasicRequestStatus(jsonInput, false);
        encodedResult = JSONCodec::encode(jsonResult);
        jsonResultArr.push_back(jsonResult);
        msecs = TimerService::getCurrentTimeMsecs();
        jsonResultArr.push_back(msecs);
        jsonResultArr.push_back(TimerService::getMsecsTimeString(msecs));
        encodedResultArr = JSONCodec::encode(jsonResultArr);
        LOG_DEBUG(CL_LOG, 
                  "invokeAndResp: Invoked on input (%s) and returned (%s)",
                  rpcInvocation.c_str(),
                  encodedResult.c_str());
        inputObj = jsonInput.get<JSONValue::JSONObject>();
        const JSONValue::JSONArray &paramArr = 
            inputObj["params"].get<JSONValue::JSONArray>();
        if (paramArr.size() == 0) {
            LOG_WARN(CL_LOG, 
                     "invokeAndResp: No params for the request, so putting "
                     "result in default completed queue (%s)",
                     m_completedQueue->getKey().c_str());
            m_completedQueue->put(encodedResultArr);
            return;
        }
        const JSONValue::JSONObject &paramObj = 
            paramArr[0].get<JSONValue::JSONObject>();
        jsonInputIt = paramObj.find(
            ClusterlibStrings::JSONOBJECTKEY_RESPQUEUEKEY);
        if (jsonInputIt != paramObj.end()) {
            string respQueueKey = 
            jsonInputIt->second.get<JSONValue::JSONString>();
            Queue *respQueue = dynamic_cast<Queue *>(
                m_root->getNotifyableFromKey(respQueueKey));
            if (respQueue != NULL) {
                respQueue->put(encodedResult);
                /*
                 * Also add to the completed queue if the
                 * defaultCompletedQueue if > 0 or -1
                 */
                if ((m_completedQueueMaxSize == -1) ||
                    (m_completedQueueMaxSize > 0)) {
                    m_completedQueue->put(encodedResultArr);
                }
            }
            else {
                LOG_WARN(CL_LOG,
                         "invokeAndResp: Tried to put result in user "
                         "selected queue (%s) and failed, so putting "
                         "result in default completed queue (%s)",
                         respQueueKey.c_str(),
                         m_completedQueue->getKey().c_str());
                m_completedQueue->put(encodedResultArr);
            }
        }
        else {
            m_completedQueue->put(encodedResultArr);
        }

        /* 
         * Try to make sure that the defaultCompletedQueue size is not
         * exceeded (this is approximate).
         */
        string tmpElement;
        while ((m_completedQueueMaxSize != -1) && 
               (m_completedQueue->size() > m_completedQueueMaxSize)) {
            m_completedQueue->takeWaitMsecs(100, tmpElement);
        }
    }
    catch (const Exception &ex) {
        JSONValue::JSONString errorString = "Caught exception: ";
        errorString.append(ex.what());
        string queueElement = JSONCodec::encode(errorString);
        LOG_WARN(CL_LOG,
                 "invokeAndResp: Couldn't parse or service command (%s) "
                 "and adding element (%s) to the DEFAULT_COMPLETED_QUEUE",
                  JSONCodec::encode(rpcInvocation).c_str(),
                 queueElement.c_str());
        m_completedQueue->put(queueElement);
    }
}

bool
ClusterlibRPCManager::setBasicRequestStatus(
    const JSONValue &jsonRequest,
    bool startingRequest,
    int32_t maxRetries)
{
    TRACE(CL_LOG, "setBasicRequestStatus");

    PropertyList *propertyList = getRPCMethodHandlerPropertyList();
    if (propertyList == NULL) {
        return false;
    }
    int32_t retries = 0;
    bool gotLock = false;
    string basicStatusKey = 
        ProcessThreadService::getHostnamePidTid() + " " + 
        ClusterlibStrings::PLK_RPCMANAGER_REQ_POSTFIX;
    JSONValue::JSONArray jsonBasicStatusArr;
    JSONValue::JSONObject jsonStatusObj;
    jsonStatusObj["request"] = jsonRequest;
    JSONValue::JSONString basicStatus;
    if (startingRequest) {
        basicStatus = "Starting the request";
    }
    else {
        basicStatus = "Finished the request";
    }
    jsonBasicStatusArr.push_back(basicStatus);
    JSONValue::JSONInteger time;
    JSONValue::JSONString timeString;
    JSONValue::JSONArray finalTimeArr;
    while ((maxRetries == -1) || (retries <= maxRetries)) {
        gotLock = propertyList->acquireLockWaitMsecs(100);
        if (gotLock) {
            time = TimerService::getCurrentTimeMsecs();
            timeString = TimerService::getMsecsTimeString(time);
            jsonBasicStatusArr.push_back(time);
            jsonBasicStatusArr.push_back(timeString);
            jsonStatusObj["basic status"] = jsonBasicStatusArr;
            propertyList->setProperty(
                basicStatusKey, 
                JSONCodec::encode(jsonStatusObj));
            try {
                propertyList->publish();
                propertyList->releaseLock();
                return true;
            }
            catch (const PublishVersionException &ex) {
                LOG_WARN(CL_LOG, 
                         "setBasicRequestStatus: Failed to update basicStatus"
                         "'%s' on try %d (PublishVersionException)",
                         basicStatusKey.c_str(),
                         retries);
            }
            propertyList->releaseLock();
        }
        ++retries;
    }

    LOG_DEBUG(CL_LOG, 
              "setBasicRequestStatus: %s %s", 
              JSONCodec::encode(jsonStatusObj).c_str(),
              JSONCodec::encode(finalTimeArr).c_str());

    return false;
}


}	/* End of 'namespace clusterlib' */
