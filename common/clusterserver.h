/*
 * clusterserver.h --
 *
 * Include file for server side types. Include this file if you are writing
 * an implementation of an application that is managed by clusterlib.
 *
 *
 * $Header$
 * $Revision$
 * $Date$
 */

#ifndef	_CLUSTERSERVER_H_
#define	_CLUSTERSERVER_H_

namespace clusterlib
{

class Server
    : public virtual Client
{
  public:
    /**
     * Retrieve the node object for "my" node.
     *
     * @return the Node * for "my" node.
     */
    Node *getMyNode() { return mp_node; }

    /**
     * Is this server managed?
     *
     * @return true if the server is managed.
     */
    bool isManaged()
    {
        return (m_flags & SF_MANAGED) ? true : false;
    }


    /**
     * \brief Registers a function that checks internal health of
     * the caller application. 
     * The given function will be called asynchronously by the cluster API
     * and will provide feedback back to the cluster.
     * 
     * @param healthChecker the callback to be used when checking for
     *                      health; if <code>NULL</code> the health
     *                      monitoring is disabled
     * @param checkFrequency how often to execute the given callback,
     *                       in seconds
     */
    void registerHealthChecker(HealthChecker *healthChecker);
    
    /**
     * \brief Retrieve the current number of seconds to wait till running
     * the health check again.
     */
    int32_t getHeartBeatPeriod();
    int32_t getUnhealthyHeartBeatPeriod();

    /**
     * \brief Enables or disables the health checking and 
     * notifies the worker thread.
     * 
     * @param enabled whether to enable the health checking
     */
    void enableHealthChecking(bool enabled);

    /**
     * \brief Participate in the leadership election protocol
     * for the containing group.
     *
     * @return true if this server became the leader of its group.
     */
    bool tryToBecomeLeader();

    /**
     * \brief Am I the leader of my group?
     *
     * @return true if this server is the leader of its group.
     */
    bool amITheLeader();

    /**
     * \brief Give up leadership of my group.
     */
    void giveUpLeadership();

  protected:
    /*
     * Make the Factory class a friend.
     */
    friend class Factory;

    /*
     * Constructor used by Factory.
     */
    Server(FactoryOps *mp_ops,
           Group *group,
           const string &nodeName,
           HealthChecker *checker,
           ServerFlags flags);

    /*
     * Destructor.
     */
    virtual ~Server();

    /**
     * Get the heart beat checking wait multiple and timeout parameters.
     */
    double getHeartBeatMultiple();
    int64_t getHeartBeatTimeout() {
	return (int64_t) (getHeartBeatMultiple() * getHeartBeatPeriod());
    }
    
    /**
     * Get the heart beat check period.
     */
    int32_t getHeartBeatCheckPeriod();
    
  private:
    /*
     * Make the default constructor private so it
     * cannot be called.
     */
    Server()
        : Client(NULL)
    {
        throw ClusterException("Someone called the Server "
                               "default constructor!");
    }

    /*
     * Periodically checks the health of the server.
     */
    void checkHealth();

    /*
     * Sets the server node health
     */
    void setNodeHealth(bool healthy);

    /**
     * \brief Called to signal whenever this cluster's node state 
     * has changed according to {@link #mp_healthChecker}.
     * 
     * @param report the new health report
     */
    void setHealthy(const HealthReport &report);

  private:
    /*
     * The factory delegate instance.
     */
    FactoryOps *mp_f;

#if 0 // AC - Ifdef'ed out until event system ready
    /**
     * The bomb handler.
     */
    TimeBombHandler m_bombHandler;
#endif

    /**
     * How often to call {@link #mpHealthChecker->checkHealth}, in seconds.
     */
    volatile int32_t m_checkFrequencyHealthy;
    volatile int32_t m_checkFrequencyUnhealthy;
    volatile double m_heartBeatMultiple;
    volatile int32_t m_heartBeatCheckPeriod;

    /*
     * Whether the checkHealth thread should terminate
     */
    volatile bool m_healthCheckerTerminating;

    /**
     * Whether the health checking is enabled.
     */
    volatile bool m_healthCheckingEnabled;

    /*
     * The object implementing health checking
     * for this "server".
     */
    HealthChecker *mp_healthChecker;

    /*
     * The thread running the health checker.
     */
    CXXThread<Server> m_checkerThread;

    /*
     * Protects healthChecker thread variables 
     */
    Mutex m_mutex;

    /*
     * Conditional variable for communicating with the healthChecker
     * thread
     */
    Cond m_cond;

    /*
     * Flags for this server.
     */
    ServerFlags m_flags;

    /*
     * The node that represents "my node".
     */
    Node *mp_node;

    /*
     * My leadership bid index.
     */
    int64_t m_myBid;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_CLUSTERSERVER_H_ */
