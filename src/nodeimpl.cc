/*
 * nodeimpl.cc --
 *
 * Implementation of the NodeImpl class.
 *
 * ============================================================================
 * $Header:$
 * $Revision$
 * $Date$
 * ============================================================================
 */

#include "clusterlibinternal.h"

#define LOG_LEVEL LOG_WARN
#define MODULE_NAME "ClusterLib"

using namespace std;

namespace clusterlib
{

#define VAL(str) #str
    
// execute the given code or die if it times out
#define LIVE_OR_DIE(code, timeout)  \
{   \
    LOG_DEBUG(CL_LOG, "Setting up a bomb to go off in %d ms if '%s' deadlocks...", timeout, VAL(code));  \
    TimerId timerId = m_timer.scheduleAfter(timeout, VAL(code));   \
    try {   \
        code;   \
        m_timer.cancelAlarm(timerId); \
    } \
    catch (std::exception &e) {   \
        m_timer.cancelAlarm(timerId); \
        LOG_ERROR(CL_LOG, \
                  "An exception while executing '%s': %s",  \
                  VAL(code), e.what()); \
    } \
    catch (...) { \
        m_timer.cancelAlarm(timerId); \
        LOG_ERROR(CL_LOG, \
                  "Unable to execute '%s', unknown exception", VAL(code));  \
    } \
    LOG_DEBUG(CL_LOG, "...disarming the bomb");  \
}

/*****************************************************************************/
/* Health management.                                                        */
/*****************************************************************************/

void
NodeImpl::registerHealthChecker(HealthChecker *healthChecker)
{
    TRACE( CL_LOG, "registerHealthChecker" );

    /*
     * Create the "connected" node.
     */
    if (!getOps()->createConnected(getKey())) {
        throw AlreadyConnectedException(
            getKey() +
            ": registerHealthChecker: Node already connected ");
    }

    if (healthChecker == NULL) {
        getOps()->removeConnected(getKey());
        throw InvalidArgumentsException(
            "registerHealthChecker: Cannot use a NULL healthChecker");
    }

    if (healthChecker->getMsecsPerCheckIfHealthy() <= 0) {
        getOps()->removeConnected(getKey());
        throw InvalidArgumentsException(
            "registerHealthChecker: Cannot have a healthy "
            "msec check cycle <= 0");
    }

    if (healthChecker->getMsecsPerCheckIfUnhealthy() <= 0) {
        getOps()->removeConnected(getKey());
        throw InvalidArgumentsException(
            "registerHealthChecker: Cannot have a unhealthy "
            "msec check cycle <= 0");
    }

    Locker l1(getHealthMutex());
    if (mp_healthChecker != NULL) {
        LOG_ERROR(CL_LOG,
                  "registerHealthChecker: Already registered healthChecker "
                  "0x%x",
                  reinterpret_cast<uint32_t>(healthChecker));
        getOps()->removeConnected(getKey());
        throw InvalidMethodException(
            "registerHealthChecker: Already registered a health checker");
    }

    m_terminateDoHealthChecks = false;
    mp_healthChecker = healthChecker;

    /*
     * Start the health checker thread
     */
    m_doHealthChecksThread.Create(*this, &NodeImpl::doHealthChecks, NULL);
}

void
NodeImpl::unregisterHealthChecker()
{
    TRACE( CL_LOG, "unregisterHealthChecker" );

    getOps()->removeConnected(getKey());

    {
        Locker l1(getHealthMutex());
        if (mp_healthChecker == NULL) {
            LOG_ERROR(CL_LOG,
                      "unregisterHealthChecker: No registered healthChecker ");
            
            throw InvalidMethodException(
                "unregisterHealthChecker: No registered health checker");
        }
        m_terminateDoHealthChecks = true;
        getHealthCond()->signal();
    }

    m_doHealthChecksThread.Join();

    Locker l1(getHealthMutex());
    mp_healthChecker = NULL;
}

NodeImpl::~NodeImpl()
{
    /*
     * Shut down health checking if the user forgot to do this.
     */
    bool stopHealthChecker = false;
    {
        Locker l1(getHealthMutex());
        if (mp_healthChecker != NULL) {
            stopHealthChecker = true;
        }
    }

    if (stopHealthChecker == true) {
        unregisterHealthChecker();
    }
}

bool
NodeImpl::isHealthy() 
{
    TRACE(CL_LOG, "isHealthy");

    Locker l1(getStateMutex());
    LOG_DEBUG(CL_LOG, 
              "isHealthy: Notifyable = (%s), clientState = (%s)",
              getKey().c_str(),
              m_clientState.c_str());

    return (m_clientState == ClusterlibStrings::HEALTHY) ? true : false;
}

void
NodeImpl::doHealthChecks(void *param)
{
    TRACE(CL_LOG, "doHealthChecks");
    
    /*
     * Initialize these to the starting values.
     */
    int64_t lastPeriod = mp_healthChecker->getMsecsPerCheckIfHealthy();
    int64_t curPeriod = mp_healthChecker->getMsecsPerCheckIfUnhealthy();

    if ((lastPeriod <= 0) || (curPeriod <= 0)) {
        throw InvalidMethodException(
            "doHealthChecks: Impossible <= 0 healthy or unhealthy period");
    }

    LOG_DEBUG(CL_LOG,
              "Starting thread with NodeImpl::doHealthChecks(), "
              "this: 0x%x, thread: 0x%x",
              (int32_t) this,
              (uint32_t) pthread_self());

    while (!m_terminateDoHealthChecks) {
        LOG_DEBUG(CL_LOG, "About to check health");

        HealthReport report(HealthReport::HS_UNHEALTHY);
        try {
#if 0 // AC - Events aren't ready yet
            //schedule an abort in 10 mins to prevent from a deadlock
            LIVE_OR_DIE(report = mp_healthChecker->checkHealth(), 
                        mp_healthChecker->getMsecsAllowedPerHealthCheck());
#else
            report = mp_healthChecker->checkHealth();		
#endif
            LOG_DEBUG(CL_LOG,
                      "doHealthChecks: Health report - state: %d, "
                      "description: %s",
                      report.getHealthState(), 
                      report.getStateDescription().c_str());
        } 
        catch (Exception &e) {
            LOG_ERROR(CL_LOG, "Caught exception: %s", e.what());
            report = HealthReport(HealthReport::HS_UNHEALTHY, e.what());
        } 
        catch (...) {
            LOG_ERROR(CL_LOG,
                      "Caught unknown exception, "
                      "assuming unhealthy state");
            report = HealthReport(HealthReport::HS_UNHEALTHY);
        }
            
        /*
         * Set the health in the repository
         */
        string value =
            (report.getHealthState() == HealthReport::HS_HEALTHY)
            ? ClusterlibStrings::HEALTHY
            : ClusterlibStrings::UNHEALTHY; 
        getOps()->updateNodeClientState(getKey(), value);
        getOps()->updateNodeClientStateDesc(getKey(), 
                                            report.getStateDescription());
        
        /*
         * Decide whether to use the CLUSTER_HEARTBEAT_HEALTHY or
         * CLUSTER_HEARTBEAT_UNHEALTHY heartbeat frequency.
         */
        lastPeriod = curPeriod;
        if (report.getHealthState() == HealthReport::HS_HEALTHY) {
            curPeriod = mp_healthChecker->getMsecsPerCheckIfHealthy();
        }
        else {
            curPeriod = mp_healthChecker->getMsecsPerCheckIfUnhealthy();
        }

        getHealthMutex()->acquire();
        LOG_DEBUG(CL_LOG,
                  "About to wait %lld msec before next health check...",
                  curPeriod);
        getHealthCond()->wait(*getHealthMutex(), 
                              static_cast<uint64_t>(curPeriod));
        LOG_DEBUG(CL_LOG, "...awoken!");

        getHealthMutex()->release();
    }
    
    LOG_DEBUG(CL_LOG,
              "Ending thread with NodeImpl::doHealthChecks(): "
              "this: 0x%x, thread: 0x%x",
              (int32_t) this,
              (uint32_t) pthread_self());
}

/*
 * Initialize the cached representation of this node.
 */
void
NodeImpl::initializeCachedRepresentation()
{
    TRACE(CL_LOG, "initializeCachedRepresentation");

    /*
     * Ensure that the cache contains all the information
     * about this node, and that all watches are established.
     */
    m_connected = getOps()->isNodeConnected(
        getKey());
    m_clientState = getOps()->getNodeClientState(
        getKey());
    m_masterSetState = getOps()->getNodeMasterSetState(
        getKey());
}

void
NodeImpl::removeRepositoryEntries()
{
    getOps()->removeNode(this);
}

};	/* End of 'namespace clusterlib' */