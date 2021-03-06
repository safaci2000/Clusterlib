/*
 * Copyright (c) 2010 Yahoo! Inc. All rights reserved. Licensed under
 * the Apache License, Version 2.0 (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. See accompanying
 * LICENSE file.
 * 
 * $Id$
 */

#include "clusterlibinternal.h"

DEFINE_LOGGER(LOG, "zookeeper.adapter")
DEFINE_LOGGER(ZK_LOG, "zookeeper.core")

/**
 * \brief A helper class to initialize ZK logging.
 */
class InitZooKeeperLogging
{
  public:
    InitZooKeeperLogging() {
        if (ZK_LOG->isDebugEnabled() || ZK_LOG->isTraceEnabled()) {
            zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
        } else if (ZK_LOG->isInfoEnabled()) {
            zoo_set_debug_level(ZOO_LOG_LEVEL_INFO);
        } else if (ZK_LOG->isWarnEnabled()) {
            zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
        } else {
            zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
        }
    }
};

using namespace std;

namespace zk {

/**
 * \brief This class provides logic for checking if a request can be retried.
 */
class RetryHandler
{
  public:
    RetryHandler(const ZooKeeperConfig &zkConfig)
        : m_zkConfig(zkConfig)
    {
        if (zkConfig.getAutoReconnect()) {
            retries = 2;
        } else {
            retries = 0;
        }
    }
        
    /**
     * \brief Attempts to fix a side effect of the given RC.
     * 
     * @param rc the ZK error code
     * @return whether the error code has been handled and the caller should 
     *         retry an operation the caused this error
     */
    bool handleRC(int32_t rc)
    {
        TRACE(LOG, "handleRC");

        //check if the given error code is recoverable
        if (!retryOnError(rc)) {
            return false;
        }
        LOG_TRACE(LOG, 
                  "RC: %d, retries left: %d", 
                  rc, 
                  retries);
        if (retries-- > 0) {
            return true;
        } else {
            return false;
        }
    }
        
  private:
    /**
     * The ZK config.
     */
    const ZooKeeperConfig &m_zkConfig;
        
    /**
     * The number of outstanding retries.
     */
    int32_t retries;    
        
    /**
     * Checks whether the given error entitles this adapter
     * to retry the previous operation.
     * 
     * @param zkErrorCode one of the ZK error code
     */
    static bool retryOnError(int32_t zkErrorCode)
    {
        return (zkErrorCode == ZCONNECTIONLOSS ||
                zkErrorCode == ZOPERATIONTIMEOUT);
    }
};
    
    
/*
 * The implementation of the global ZK event watcher
 */
void zkWatcher(zhandle_t *zhp, 
               int32_t type, 
               int32_t state, 
               const char *path, 
               void *watcherCtx)
{
    TRACE(LOG, "zkWatcher");

    LOG_DEBUG(LOG,
              "zkWatcher: Received a ZK event - type: %s, state: %s, path: "
              "'%s', context: '%p', watcherCtx: '%p'",
              zk::ZooKeeperAdapter::getEventString(type).c_str(), 
              zk::ZooKeeperAdapter::getStateString(state).c_str(), 
              path, 
              zoo_get_context(zhp),
              watcherCtx);

    ZooKeeperAdapter *zkap = (ZooKeeperAdapter *)zoo_get_context(zhp);
    if (zkap != NULL) {
        /* 
         * If the watcherCtx is the pointer to the ZooKeeperAdapter,
         * then do not pass it as the context, since the event expects
         * a UserContextAndListener pointer. 
         */
        if (watcherCtx == zoo_get_context(zhp)) {
            watcherCtx = NULL;
        }
        zkap->enqueueEvent(type, state, path, watcherCtx);
    } else {
        LOG_ERROR(LOG,
                  "Skipping ZK event (type: %s, state: %s, path: '%s'), "
                  "because ZK passed no context",
                  zk::ZooKeeperAdapter::getEventString(type).c_str(), 
                  zk::ZooKeeperAdapter::getStateString(state).c_str(),
                  path);
    }
}

// =======================================================================

string
ZooKeeperAdapter::getEventString(int32_t etype)
{
    string res;
    
    if (etype == ZOO_CHANGED_EVENT) {
        res = "ZOO_CHANGED_EVENT";
    }
    else if (etype == ZOO_CHILD_EVENT) {
        res = "ZOO_CHILD_EVENT";
    }
    else if (etype == ZOO_CREATED_EVENT) {
        res = "ZOO_CREATED_EVENT";
    }
    else if (etype == ZOO_DELETED_EVENT) {
        res = "ZOO_DELETED_EVENT";
    }
    else if (etype == ZOO_NOTWATCHING_EVENT) {
        res = "ZOO_NOTWATCHING_EVENT";
    }
    else if (etype == ZOO_SESSION_EVENT) {
        res = "ZOO_SESSION_EVENT";
    }
    else {
        res = "unknown event type";
    }

    return res;
}

string
ZooKeeperAdapter::getStateString(int32_t state)
{
    string res;

    if (state == ZOO_EXPIRED_SESSION_STATE) {
        res = "ZOO_EXPIRED_SESSION_STATE";
    }
    else if (state == ZOO_AUTH_FAILED_STATE) {
        res = "ZOO_AUTH_FAILED_STATE";
    }
    else if (state == ZOO_CONNECTING_STATE) {
        res = "ZOO_CONNECTING_STATE";
    }
    else if (state == ZOO_ASSOCIATING_STATE) {
        res = "ZOO_ASSOCIATING_STATE";
    }
    else if (state == ZOO_CONNECTED_STATE) {
        res = "ZOO_CONNECTED_STATE";
    }
    else {
        res = "unknown state";
    }

    return res;
}

void
ZooKeeperAdapter::splitSequenceNode(const string &sequenceNode,
                                    string *pSequenceName,
                                    int64_t *pSequenceNumber)
{
    TRACE(LOG, "splitSequentialNode");

    if (sequenceNode.size() < 
        clusterlib::CLNumericInternal::SEQUENCE_NUMBER_SIZE) {
        throw InconsistentInternalStateException(
            string("splitSequentialNode: Node ") +
            sequenceNode + " is too small to split");
    }

    /*
     * Set the name properly
     */
    if (pSequenceName != NULL) {
        *pSequenceName = sequenceNode.substr(
            0, 
             sequenceNode.size() -
            clusterlib::CLNumericInternal::SEQUENCE_NUMBER_SIZE);
    }

    /*
     * Set and ensure that this is a legal sequence number.
     */
    if (pSequenceNumber != NULL) {
        *pSequenceNumber = ::strtol(
            &(sequenceNode.c_str()[
                  sequenceNode.size() - 
                  clusterlib::CLNumericInternal::SEQUENCE_NUMBER_SIZE]), 
            NULL, 
            10);
        if (*pSequenceNumber < 0) {
            LOG_WARN(LOG, 
                     "splitSequentialNode: Expecting a valid number "
                     "but got %s", 
                     &(sequenceNode.c_str()[
                           sequenceNode.size() -    
                           clusterlib::CLNumericInternal::SEQUENCE_NUMBER_SIZE]));
            throw InconsistentInternalStateException(
                string("splitSequentialNode: Expecting a valid number "
                       "but got ") +
                &(sequenceNode.c_str()[
                      sequenceNode.size() -
                      clusterlib::CLNumericInternal::SEQUENCE_NUMBER_SIZE]));
        }
    }
}

void ZooKeeperAdapter::throwErrorCode(const string &msg,
                                      int32_t errorCode,
                                      bool connected) 
{
    string errorCodeMsg = msg;
    switch(errorCode) {
        case ZOK:
            errorCodeMsg.append(" (ZOK)");
            break;
        case ZNONODE:
            errorCodeMsg.append(" (ZNONODE)");
            break;
        case ZNODEEXISTS:
            errorCodeMsg.append(" (ZNODEEXISTS)");
            break;
        case ZNOAUTH:
            errorCodeMsg.append(" (ZNOAUTH)");
            throw NoAuthException(errorCodeMsg, errorCode, connected);
        case ZNOCHILDRENFOREPHEMERALS:
            errorCodeMsg.append(" (ZNOCHILDRENFOREPHEMERALS)");
            break;
        case ZINVALIDSTATE:
            errorCodeMsg.append(" (ZINVALIDSTATE)");
            throw InvalidStateException(errorCodeMsg, errorCode, connected);
        case ZBADVERSION:
            errorCodeMsg.append(" (ZBADVERSION)");
            throw BadVersionException(errorCodeMsg, errorCode, connected);
        case ZBADARGUMENTS:
            errorCodeMsg.append(" (ZBADARGUMENTS)");
            break;
        case ZMARSHALLINGERROR:
            errorCodeMsg.append(" (ZMARSHALLINGERROR)");
            break;
        default:
            errorCodeMsg.append(" (unknown)");
    }
    throw UnknownErrorCodeException(errorCodeMsg, errorCode, connected);
}

ZooKeeperAdapter::ZooKeeperAdapter(ZooKeeperConfig config, 
                                   ZKEventListener *lp,
                                   bool establishConnection) 
    : m_zkConfig(config),
      mp_zkHandle(NULL), 
      m_connected(false),
      m_state(AS_DISCONNECTED),
      m_eventDispatchAllowed(true)
{
    TRACE(LOG, "ZooKeeperAdapter");

    resetRemainingConnectTimeout();
    
    //enforce setting up appropriate ZK log level
    static InitZooKeeperLogging INIT_ZK_LOGGING;
    
    if (lp != NULL) {
        addListener(lp);
    }

    //start the event dispatcher thread
    m_eventDispatcher.Create(*this, &ZooKeeperAdapter::processEvents);

    //start the user event dispatcher thread
    m_userEventDispatcher.Create(*this, &ZooKeeperAdapter::processUserEvents);
    
    //optionally establish the connection
    if (establishConnection) {
        reconnect();
    }
}

ZooKeeperAdapter::~ZooKeeperAdapter()
{
    TRACE(LOG, "~ZooKeeperAdapter");

    try {
        disconnect(true);
    } catch (std::exception &e) {
        LOG_ERROR(LOG, 
                  "An exception while disconnecting from ZK: %s",
                  e.what());
    }

    /* Exit our threads */
    m_userEventDispatcher.Join();
    m_eventDispatcher.Join();

    /* 
     * Clean up the CallbackAndContext objects not sent back with a
     * watch fire. 
     */
    getListenerAndContextManager()->deleteAllCallbackAndContext();
}

void
ZooKeeperAdapter::validatePath(const string &path)
{
    TRACE(LOG, "validatePath");
    
    if (path.find("/") != 0) {
        throw InvalidArgumentsException(
            string("Node path must start with '/' but") +
            string(" it was '") + path + "'");
    }
    if (path.length() > 1) {
        if (path.rfind("/") == path.length() - 1) {
            throw InvalidArgumentsException(
                string("Node path must not end with "
                       "'/' but it was '") + path + "'");
        }
        if (path.find("//") != string::npos) {
            throw InvalidArgumentsException(
                string("Node path must not contain "
                       "'//' but it was '") + path + "'");
        }
    }
}

void
ZooKeeperAdapter::disconnect(bool final)
{
    TRACE(LOG, "disconnect");

    LOG_TRACE(LOG, 
              "mp_zkHandle: %p, state %d", 
              mp_zkHandle, m_state);

    m_stateLock.lock();
    if (mp_zkHandle != NULL) {
        int32_t ret = zookeeper_close(mp_zkHandle);
        mp_zkHandle = NULL;
        if (final == true) {
            setState(AS_NORECONNECT);
            /* 
             * Pass synthetic end event into the event queue to have
             * cascading thread exit.  It is okay for this to have
             * been called before.  The end event will only be
             * delivered once.
             */
            injectEndEvent();
        }
        else {
            setState(AS_DISCONNECTED);
        }
        LOG_INFO(LOG, "disconnect: closed with ret = %d", ret);
    }
    m_stateLock.unlock();
}

void
ZooKeeperAdapter::stopEventDispatch()
{
    m_eventDispatchAllowed = false;
}

void
ZooKeeperAdapter::reconnect()
{
    TRACE(LOG, "reconnect");
    
    m_stateLock.lock();
    if (m_state == AS_NORECONNECT) {
        m_stateLock.unlock();
        throw InvalidMethodException(
            "reconnect: Failed since no reconnection is allowed!");
    }
    /* Clear the connection state */
    disconnect();
    
    LOG_INFO(LOG, 
             "reconnect: Making a connection to %s with a timeout of %" 
             PRId64 " msecs",
             m_zkConfig.getHosts().c_str(),
             m_zkConfig.getConnectTimeout());
    
    /* Establish a new connection to ZooKeeper */
    mp_zkHandle = zookeeper_init(m_zkConfig.getHosts().c_str(), 
                                 zkWatcher, 
                                 m_zkConfig.getConnectTimeout(),
                                 0,
                                 this, 
                                 0);
    resetRemainingConnectTimeout();
    if (mp_zkHandle != NULL) {
        setState(AS_CONNECTING);
        m_stateLock.unlock();
    } else {
        m_stateLock.unlock();
        throw Exception(
	    string("Unable to connect to ZK running at '") +
            m_zkConfig.getHosts() + "'");
    }
    
    LOG_DEBUG(LOG, 
              "mp_zkHandle: %p, state %d", 
              mp_zkHandle, 
              m_state); 
}

struct sync_completion {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool done;
    int32_t rc;
    ZooKeeperAdapter *zk;
};

static void waitCompletion(int32_t rc, const char *value, const void *data)
{
    int32_t ret;
    
    struct sync_completion *scp = (struct sync_completion *) data;
    scp->rc = rc;
    ret = pthread_mutex_lock(&scp->mutex);
    if (ret) {
	throw SystemFailureException("Unable to lock mutex");
    }
    scp->done =true;
    ret = pthread_cond_signal(&scp->cond);
    if (ret) {
	throw SystemFailureException("Unable to wait cond");
    }    
    ret = pthread_mutex_unlock(&scp->mutex);
    if (ret) {
	throw SystemFailureException("Unable to unlock mutex");
    }
}

bool
ZooKeeperAdapter::sync(const string &path,
                       ZKEventListener *listener,
                       void *context)
{
    TRACE(LOG, "sync");

    validatePath(path);

    int32_t rc = ZINVALIDSTATE, ret;
    struct sync_completion sc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
	memset(&sc, 0, sizeof(sc));
	sc.zk = this;
	ret = pthread_mutex_init(&sc.mutex, NULL);
	if (ret) {
	    throw SystemFailureException("Unable to init mutex");
	}
	ret = pthread_cond_init(&sc.cond, NULL); 
	if (ret) {
	    throw SystemFailureException("Unable to init cond");
	}
	rc = zoo_async(mp_zkHandle,
                       path.c_str(),
                       waitCompletion,
                       &sc);
	if (rc == ZOK) {	    
	    ret = pthread_mutex_lock(&sc.mutex);
	    if (ret) {
		throw SystemFailureException("Unable to lock mutex");
	    }
	    while (!sc.done) {
		ret = pthread_cond_wait(&sc.cond, &sc.mutex);
		if (ret) {
		    throw SystemFailureException("Unable to wait cond");
		}
	    }
	    ret = pthread_mutex_unlock(&sc.mutex);
	    if (ret) {
		throw SystemFailureException("Unable to unlock mutex");
	    }
	}

	ret = pthread_mutex_destroy(&sc.mutex);
	if (ret) {
	    throw SystemFailureException("Unable to destroy mutex");
	}
	ret = pthread_cond_destroy(&sc.cond); 
	if (ret) {
	    throw SystemFailureException("Unable to destroy cond");
	}
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        LOG_ERROR(LOG, 
                  "sync: Error %d for %s", 
                  rc, 
                  path.c_str());
        throwErrorCode("Unable to sync data for node ",
                       rc,
                       m_state == AS_CONNECTED);
    }

    /* Sync cannot set a watch, so manually push a zk event up to the
     * listeners.  This assumes:
     * 
     * 1) After a sync, all the watches for any other events have been
     * triggered and processed by zkWatcher.  
     * 2) Syncs complete in order. 
     * 
     * At this point, we insert the sync event into the blocking queue
     * for other listeners.
     */

    /*
     * Allocate the struct passed in as context for.  It will be
     * deallocated when the event is processed through the watcher
     * function.
     */
    clusterlib::CallbackAndContext *callbackAndContext =
        getListenerAndContextManager()->createCallbackAndContext(listener,
                                                                 context);
    m_events.put(ZKWatcherEvent(ZOO_SESSION_EVENT, 
                                ZOO_CONNECTED_STATE,
                                clusterlib::CLStringInternal::SYNC,
                                callbackAndContext));
    return true;
}

void
ZooKeeperAdapter::handleAsyncEvent(const ZKWatcherEvent &event)
{
    TRACE(LOG, "handleAsyncEvent");

    LOG_DEBUG(LOG, 
              "handleAsyncEvent: type: %s, state %s, path: %s, context: %p",
              getEventString(event.getType()).c_str(),
              getStateString(event.getState()).c_str(),
              event.getPath().c_str(),
              event.getContext());

    /* 
     * If there is a context, then it should be of type
     * UserContextAndListener.  If it has a listener, then send the
     * event to on that listener.  Extract the user context if there
     * is one.  Otherwise, send it to all listeners.
     */
    ZKEventListener *listener = NULL;
    void *userContext = NULL;
    clusterlib::CallbackAndContext *callbackAndContext = NULL;
    if (event.getContext() != NULL) {
        callbackAndContext = 
            reinterpret_cast<clusterlib::CallbackAndContext *>(
                event.getContext());
        listener = 
            reinterpret_cast<ZKEventListener *>(callbackAndContext->callback);
        userContext = callbackAndContext->context;
    }

    ZKWatcherEvent sentEvent(event.getType(), 
                             event.getState(), 
                             event.getPath(),
                             userContext);
    if (listener != NULL) {
        fireEvent(listener, sentEvent);
    }
    else {
        fireEventToAllListeners(sentEvent);
    }

    /*
     * Clean up the context struct from memory.
     */
    if (callbackAndContext != NULL) {
        getListenerAndContextManager()->
            deleteCallbackAndContext(callbackAndContext);
    }
}

void
ZooKeeperAdapter::injectEndEvent()
{
    m_events.put(ZKWatcherEvent(
                     ZOO_SESSION_EVENT,
                     ZOO_EXPIRED_SESSION_STATE,
                     clusterlib::CLStringInternal::END_EVENT.c_str(),
                     NULL));
}

bool
ZooKeeperAdapter::isEndEvent(const ZKWatcherEvent &event) const
{
    if ((event.getType() == ZOO_SESSION_EVENT) &&
        (event.getState() == ZOO_EXPIRED_SESSION_STATE) &&
        (event.getPath().compare(clusterlib::CLStringInternal::END_EVENT)
         == 0) &&
        (event.getContext() == NULL)) {
        return true;
    }
    else {
        return false;
    }
}

void 
ZooKeeperAdapter::enqueueEvent(int32_t type,
                               int32_t state,
                               const string &path,
                               ContextType context)
{
    TRACE(LOG, "enqueueEvents");

    /*
     * Drop the event if dispatch is not allowed.
     */
    if (m_eventDispatchAllowed == false) {
        return;
    }

    /*
     * Pass the event to the handler.
     */
    m_events.put(ZKWatcherEvent(type, state, path, context));
}

void
ZooKeeperAdapter::processEvents(void *param)
{
    TRACE(LOG, "processEvents");

    LOG_DEBUG(LOG,
              "Starting thread with ZooKeeperAdapter::processEvents(), "
              "this: %p, thread: %" PRIu32,
              this,
              clusterlib::ProcessThreadService::getTid());

    while (1) {
        ZKWatcherEvent source;
        bool found = m_events.takeWaitMsecs(100, source);
        if (found) {
            if (source.getType() == ZOO_SESSION_EVENT) {
                LOG_INFO(LOG,
                         "processEvents: Received SESSION event, state: %s. "
                         "Adapter state: %s (%d)",
                         getStateString(source.getState()).c_str(),
                         ZooKeeperAdapter::getStateString(m_state).c_str(),
                         m_state);
                m_stateLock.lock();
                if (source.getState() == ZOO_CONNECTED_STATE) {
                    resetRemainingConnectTimeout();
                    setState(AS_CONNECTED);
                } 
                else if (source.getState() == ZOO_CONNECTING_STATE) {
                    setState(AS_CONNECTING);
                } 
                else if (source.getState() == ZOO_EXPIRED_SESSION_STATE) {
                    LOG_INFO(LOG, 
                             "processEvents: Received EXPIRED_SESSION event "
                             "with path %s",
                             source.getPath().c_str());
                    /*
                     * ZOO_EXPIRED_SESSION_STATE is overloaded to
                     * specify an end event for FactoryOps.
                     */
                    if (source.getPath().compare(
                            clusterlib::CLStringInternal::END_EVENT) != 0) {
                        setState(AS_SESSION_EXPIRED);
                    }
                }
                m_stateLock.unlock();
            }

            LOG_DEBUG(LOG,
                      "processEvents: Received event, type: %s, state: %s, "
                      "path: %s, adapter state: %s (%d)",
                      getEventString(source.getType()).c_str(), 
                      getStateString(source.getState()).c_str(),
                      source.getPath().c_str(), 
                      ZooKeeperAdapter::getStateString(m_state).c_str(),
                      m_state);

            m_userEvents.put(source);
            
            /* If that was the final event, exit loop */
            if (isEndEvent(source)) {
                break;
            }
        }
    }

    LOG_INFO(LOG,
             "Ending thread with ZooKeeperAdapter::processEvents(): "
             "this: %p, thread: %" PRIu32,
             this,
             clusterlib::ProcessThreadService::getTid());
}

void
ZooKeeperAdapter::processUserEvents(void *param)
{
    TRACE(LOG, "processUserEvents");

    LOG_INFO(LOG,
             "Starting thread with ZooKeeperAdapter::processUserEvents(), "
             "this: %p, thread: %" PRIu32,
             this,
             clusterlib::ProcessThreadService::getTid());

    while (1) {
        ZKWatcherEvent source;
        bool found = m_userEvents.takeWaitMsecs(100, source);
        if (found) {
            try {
		LOG_DEBUG(LOG,
                          "processUserEvents: processing event (type: %s, "
                          "state: %s, path: %s, context %p)",
                          getEventString(source.getType()).c_str(),
                          getStateString(source.getState()).c_str(),
                          source.getPath().c_str(),
                          source.getContext());
		
                handleAsyncEvent(source);
            } 
            catch (std::exception &e) {
                LOG_ERROR(LOG, 
                          "Unable to process event (type: %s, state: %s, "
                          "path: %s), because of exception: %s",
                          getEventString(source.getType()).c_str(),
                          getStateString(source.getState()).c_str(),
                          source.getPath().c_str(),
                          e.what());
            }

            /* If that was the final event, exit loop */
            if (isEndEvent(source)) {
                break;
            }
        }
    }

    LOG_INFO(LOG,
             "Ending thread with ZooKeeperAdapter::processUserEvents() "
             "this: %p, thread: %" PRIu32,
             this,
             clusterlib::ProcessThreadService::getTid());
}

void 
ZooKeeperAdapter::setState(AdapterState newState)
{
    TRACE(LOG, "setState");    
    if (newState != m_state) {
        LOG_INFO(LOG, 
                 "Adapter state transition: %d (%s) -> %d (%s)", 
                 m_state, 
                 getStateString(m_state).c_str(),
                 newState,
                 getStateString(newState).c_str());
        m_state = newState;
        m_connected = (newState == AS_CONNECTED);
        m_stateLock.notify();
    } else {
        LOG_TRACE(LOG,
                  "New state same as the current: %d (%s)", 
                  newState,
                  getStateString(newState).c_str());
    }
}

//TODO move this code to verifyConnection so reconnect()
//is called from one place only
void
ZooKeeperAdapter::waitUntilConnected() 
{
    TRACE(LOG, "waitUntilConnected");    
    int64_t timeout = getRemainingConnectTimeout();
    LOG_INFO(LOG,
             "Waiting up to %" PRId64
             " ms until a connection to ZK is established",
             timeout);
    bool connected;
    if (timeout > 0) {
        int64_t toWait = timeout;
        while (m_state != AS_CONNECTED && toWait > 0) {
            //check if session expired and reconnect if so
            if (m_state == AS_SESSION_EXPIRED) {
                LOG_INFO(LOG,
                         "Reconnecting because the current session "
                         "has expired");
                reconnect();
            }
            struct timeval now;
            gettimeofday(&now, NULL);
            int64_t milliSecs = -(now.tv_sec * 1000LL + now.tv_usec / 1000);
            LOG_TRACE(LOG, 
                      "About to wait %" PRId64 " ms", 
                      toWait);
            m_stateLock.waitMsecs(toWait);
            gettimeofday(&now, NULL);
            milliSecs += now.tv_sec * 1000LL + now.tv_usec / 1000;
            toWait -= milliSecs;
        }
        waitedForConnect(timeout - toWait);
        LOG_INFO(LOG, 
                 "Waited %" PRId64 " ms", 
                 timeout - toWait);
    }
    connected = (m_state == AS_CONNECTED);
    if (!connected) {
        if (timeout > 0) {
            LOG_WARN(LOG, 
                     "Timed out while waiting for connection to ZK");
            throw Exception("Timed out while waiting for connection to ZK");
        } else {
            LOG_ERROR(LOG, 
                      "Global timeout expired and "
                      "still not connected to ZK");
            throw Exception("Global timeout expired and still not "
                            "connected to ZK");
        }
    }
    LOG_INFO(LOG, "Connected!");
}

void
ZooKeeperAdapter::verifyConnection()
{
    TRACE(LOG, "verifyConnection");

    m_stateLock.lock();
    try {
        if (m_state == AS_DISCONNECTED) {
            throw Exception(
                "Disconnected from ZK. "
                "Please use reconnect() before attempting to use any ZK API");
        } 
        else if (m_state != AS_CONNECTED) {
            LOG_DEBUG(LOG, 
                      "verifyConnection: Checking if need to reconnect...");
            //we are not connected, so check if connection in progress...
            if (m_state != AS_CONNECTING) {
                LOG_DEBUG(LOG, 
                          "yes. Checking if allowed to auto-reconnect...");
                //...not in progres, so check if we can reconnect
                if (!m_zkConfig.getAutoReconnect()) {
                    //...too bad, disallowed :(
                    LOG_DEBUG(LOG, "no. Sorry.");
                    throw Exception("ZK connection is down and "
                                    "auto-reconnect is not allowed");
                } 
                else {
                    LOG_DEBUG(LOG, 
                              "...yes. About to reconnect");
                }
                //...we are good to retry the connection
                reconnect();
            } 
            else {
                LOG_DEBUG(LOG, 
                          "...no, already in CONNECTING state");
            }               
            //wait until the connection is established
            waitUntilConnected(); 
        }

        /*
         * Connection should be good
         */
    } catch (ZooKeeperException &e) {
        m_stateLock.unlock();
        throw;
    }
    m_stateLock.unlock();
}

bool
ZooKeeperAdapter::createNode(const string &path, 
                             const string &value, 
                             int32_t flags, 
                             bool createAncestors,
                             string &createdPath) 
{
    TRACE(LOG, "createNode (internal)");
    validatePath(path);
    
    const int32_t MAX_PATH_LENGTH = 1024;
    char realPath[MAX_PATH_LENGTH];
    realPath[0] = 0;
    
    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        rc = zoo_create(mp_zkHandle, 
                        path.c_str(), 
                        value.c_str(),
                        value.length(),
                        &ZOO_OPEN_ACL_UNSAFE,
                        flags,
                        realPath,
                        MAX_PATH_LENGTH);
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        if (rc == ZNODEEXISTS) {
            //the node already exists
            LOG_WARN(LOG, 
                     "createNode: Error %d for %s", 
                     rc, 
                     path.c_str());
            return false;
        } 
        else if (rc == ZNONODE && createAncestors) {
            LOG_WARN(LOG, 
                     "createNode: Error %d for %s", 
                     rc, 
                     path.c_str());
            //one of the ancestors doesn't exist so lets start from the root 
            //and make sure the whole path exists, creating missing nodes if
            //necessary
            for (string::size_type pos = 1; pos != string::npos;) {
                pos = path.find("/", pos);
                if (pos != string::npos) {
                    try {
                        createNode(path.substr(0, pos), "", 0, true);
                    } catch (Exception &e) {
                        throw Exception(string("Unable to create "
                                                        "node ") + 
                                                 path, 
                                                 rc,
                                                 m_state == AS_CONNECTED);
                    }
                    pos++;
                } else {
                    //no more path components
                    return createNode(path, value, flags, false, createdPath);
                }
            }
        }
        else {
            LOG_ERROR(LOG,
                      "createNode: Error %d for %s", 
                      rc, 
                      path.c_str());
            throwErrorCode(string("Unable to create node ") +
                           path,
                           rc,
                           m_state == AS_CONNECTED);
        }
    } 

    LOG_DEBUG(LOG, 
              "%s has been created", 
              realPath);
    createdPath = string(realPath);
    return true;
}

bool
ZooKeeperAdapter::createNode(const string &path,
                             const string &value,
                             int32_t flags,
                             bool createAncestors) 
{
    TRACE(LOG, "createNode");

    string createdPath;
    return createNode(path, value, flags, createAncestors, createdPath);
}

int64_t
ZooKeeperAdapter::createSequence(const string &path,
                                 const string &value,
                                 int32_t flags,
                                 bool createAncestors,
                                 string &createdPath) 
{
    TRACE(LOG, "createSequence");

    bool result = createNode(path,
                             value,
                             flags | ZOO_SEQUENCE,
                             createAncestors,
                             createdPath);
    if (!result) {
        return -1;
    } else {
        /* 
         * Extract sequence number from the returned path.
         */
        if (createdPath.find(path) != 0) {
            throw InconsistentInternalStateException(
                string("Expecting returned path '") +
                createdPath + "' to start with '" + path + "'");
        }
        string seqSuffix =
            createdPath.substr(path.length(), 
                               createdPath.length() - path.length());
        char *ptr = NULL;
        int64_t seq = strtol(seqSuffix.c_str(), &ptr, 10);
        if (ptr != NULL && *ptr != '\0') {
            throw InconsistentInternalStateException(
                string("Expecting a number but got ") + seqSuffix);
        }
        return seq;
    }
}

bool
ZooKeeperAdapter::deleteNode(const string &path,
                             bool recursive,
                             int32_t version)
{
    TRACE(LOG, "deleteNode");

    validatePath(path);
        
    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        rc = zoo_delete(mp_zkHandle, path.c_str(), version);
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        if (rc == ZNONODE) {
            LOG_WARN(LOG, 
                     "Error %d for %s", 
                     rc, 
                     path.c_str());
            return false;
        }
        if (rc == ZNOTEMPTY && recursive) {
            LOG_WARN(LOG, 
                     "Error %d for %s", 
                     rc, 
                     path.c_str());
            //get all children and delete them recursively...
            vector<string> nodeList;
            getNodeChildren(path, nodeList, false);
            for (vector<string>::const_iterator i = nodeList.begin();
                 i != nodeList.end();
                 ++i) {
                deleteNode(*i, true);
            }
            //...and finally attempt to delete the node again
            return deleteNode(path, false); 
        }
        LOG_ERROR(LOG, 
                  "deleteNode: Error %d for %s", 
                  rc, 
                  path.c_str());
        throwErrorCode(string("Unable to delete node ") + path,
                              rc,
                              m_state == AS_CONNECTED);
    } 

    LOG_DEBUG(LOG, 
              "%s has been deleted", 
              path.c_str());
    return true;
}

bool
ZooKeeperAdapter::nodeExists(const string &path,
                             ZKEventListener *listener,
                             void *context, 
                             Stat *stat)
{
    TRACE(LOG, "nodeExists");

    validatePath(path);

    struct Stat tmpStat;
    if (stat == NULL) {
        stat = &tmpStat;
    }
    memset(stat, 0, sizeof(Stat));

    int32_t rc;
    RetryHandler rh(m_zkConfig);

    LOG_DEBUG(LOG,
              "nodeExists: path (%s), listener (%p), "
              "context (%p), stat (%p)\n",
              path.c_str(),
              listener,
              context,
              stat);

    /*
     * Allocate the struct passed in as context for zoo_wexists().  It
     * will be deallocated when the event is processed through the
     * watcher function.
     */
    clusterlib::CallbackAndContext *callbackAndContext =
        getListenerAndContextManager()->createCallbackAndContext(listener,
                                                                 context);
    do {
        verifyConnection();
        if (listener == NULL) {
            rc = zoo_exists(mp_zkHandle,
                            path.c_str(),
                            0,
                            stat);
        }
        else {
            rc = zoo_wexists(mp_zkHandle,
                             path.c_str(),
                             zkWatcher,
                             callbackAndContext,
                             stat);
        }
    } while (((rc != ZOK) && (rc != ZNONODE)) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        if (rc == ZNONODE) {
            LOG_DEBUG(LOG, 
                      "Node %s does not exist", 
                      path.c_str());
            return false;
        }
        LOG_ERROR(LOG, 
                  "nodeExists: Error %d for %s", 
                  rc, 
                  path.c_str());
        getListenerAndContextManager()->deleteCallbackAndContext(
            callbackAndContext);

        throwErrorCode(
            string("Unable to check existence of node ") + path,
            rc,
            m_state == AS_CONNECTED);
    } 

    return true;        
}

bool
ZooKeeperAdapter::getNodeChildren(const string &path, 
                                  vector<string> &nodeList,
                                  ZKEventListener *listener,
                                  void *context)
{
    TRACE(LOG, "getNodeChildren");

    validatePath(path);
    
    String_vector children;
    memset(&children, 0, sizeof(children));

    int32_t rc;
    RetryHandler rh(m_zkConfig);

    LOG_DEBUG(LOG,
              "getNodeChildren: path (%s), listener (%p), "
              "context (%p)\n",
              path.c_str(),
              listener,
              context);

    /*
     * Allocate the struct passed in as context for
     * zoo_wget_children().  It will be deallocated when the event is
     * processed through the watcher function.
     */    
    clusterlib::CallbackAndContext *callbackAndContext =
        getListenerAndContextManager()->createCallbackAndContext(listener,
                                                                 context);
    do {
        verifyConnection();
        if (listener == NULL) {
            rc = zoo_get_children(mp_zkHandle,
                                  path.c_str(), 
                                  0,
                                  &children);
        }
        else {
            rc = zoo_wget_children(mp_zkHandle,
                                   path.c_str(), 
                                   zkWatcher,
                                   callbackAndContext,
                                   &children);
        }
    } while (((rc != ZOK) && (rc != ZNONODE)) && (rh.handleRC(rc)));
    nodeList.clear();
    if ((rc != ZOK) && (rc != ZNONODE)) {
        LOG_ERROR(LOG, 
                  "getNodeChildren: Error %d for %s", 
                  rc, 
                  path.c_str());
        getListenerAndContextManager()->deleteCallbackAndContext(
            callbackAndContext);
        throwErrorCode(string("Unable to get children of node ") +
                              path, 
                              rc,
                              m_state == AS_CONNECTED);
    } 

    if (listener == NULL) {
        getListenerAndContextManager()->deleteCallbackAndContext(
            callbackAndContext);
    }
    
    if (rc == ZNONODE) {
        return false;
    }
    
    for (int32_t i = 0; i < children.count; ++i) {
        /*
         * Convert each child's path from relative to absolute.
         */
        string absPath(path);
        if (path != "/") {
            absPath.append("/");
        } 
        absPath.append(children.data[i]); 
        nodeList.push_back(absPath);
    }

    /*
     * Make sure the order is always deterministic.  Note: Is this
     * necessary?
     */
    sort(nodeList.begin(), nodeList.end());
    return true;
}

/*
  listener here is an alternative listener.  If chosen then the events
  will not only go to this listener, not any default listeners.
 */

bool
ZooKeeperAdapter::getNodeData(const string &path,
                              string &data,
                              ZKEventListener *listener,
                              void *context, 
                              Stat *stat)
{
    TRACE(LOG, "getNodeData");

    validatePath(path);
   
    const int32_t MAX_DATA_LENGTH = 1024 * 1024;
    char *buffer = new char[MAX_DATA_LENGTH];
    memset(buffer, 0, MAX_DATA_LENGTH);
    struct Stat tmpStat;
    if (stat == NULL) {
        stat = &tmpStat;
    }
    memset(stat, 0, sizeof(Stat));
    
    int32_t rc;
    int32_t len;
    RetryHandler rh(m_zkConfig);

    /*
     * Allocate the struct passed in as context for zoo_wget().  It
     * will be deallocated when the event is processed through the
     * watcher function.
     */
    clusterlib::CallbackAndContext *callbackAndContext =
        getListenerAndContextManager()->createCallbackAndContext(listener,
                                                                 context);
    do {
        verifyConnection();
        len = MAX_DATA_LENGTH - 1;
        if (listener == NULL) {
            rc = zoo_get(mp_zkHandle, 
                         path.c_str(),
                         0,
                         buffer, 
                         &len, 
                         stat);
        }
        else {
            rc = zoo_wget(mp_zkHandle, 
                          path.c_str(),
                          zkWatcher,
                          callbackAndContext,
                          buffer, 
                          &len, 
                          stat);
        }
    } while (((rc != ZOK) && (rc != ZNONODE)) && (rh.handleRC(rc)));
    data.clear();
    if ((rc != ZOK) && (rc != ZNONODE)) {
        LOG_ERROR(LOG, 
                  "getNodeData: Error %d for %s", 
                  rc, 
                  path.c_str());
        delete [] buffer;
        getListenerAndContextManager()->deleteCallbackAndContext(
            callbackAndContext);
        throwErrorCode(
            string("Unable to get data of node ") + path,
            rc,
            m_state == AS_CONNECTED);
    } 

    LOG_DEBUG(LOG,
              "getNodeData: path (%s), listener (%p), "
              "context (%p), stat (%p), data (%s)\n",
              path.c_str(),
              listener,
              context,
              stat,
              buffer);
    
    if (listener == NULL) {
        getListenerAndContextManager()->deleteCallbackAndContext(
            callbackAndContext);
    }
    
    if (rc == ZOK) {
        data.assign(buffer, len);
        delete [] buffer;
        return true;
    }
    else {
        delete [] buffer;
        return false;
    }
}

void
ZooKeeperAdapter::setNodeData(const string &path,
                              const string &value,
                              int32_t version,
                              Stat *stat)
{
    TRACE(LOG, "setNodeData");

    validatePath(path);

    int32_t rc;
    RetryHandler rh(m_zkConfig);

    LOG_DEBUG(LOG,
              "setNodeData: path (%s), value (%s), version (%d)\n",
              path.c_str(),
              value.c_str(),
              version);

    do {
        verifyConnection();
        rc = zoo_set2(mp_zkHandle,
                      path.c_str(),
                      value.c_str(),
                      value.length(), 
                      version,
                      stat);
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        LOG_ERROR(LOG, 
                  "setNodeData: Error %d for %s", 
                  rc, 
                  path.c_str());
        throwErrorCode("setNodeData: Failed",
                       rc,
                       m_state == AS_CONNECTED);
    }
}

}   /* end of 'namespace zk' */

