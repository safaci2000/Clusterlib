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
#include "testparams.h"
#include "MPITestFixture.h"

extern TestParams globalTestParams;

using namespace std;
using namespace boost;
using namespace clusterlib;
using namespace json;

const string appName = "unittests-healthCheck-app";

class ClusterlibHealthCheck : public MPITestFixture
{
    CPPUNIT_TEST_SUITE(ClusterlibHealthCheck);
    CPPUNIT_TEST(testHealthCheck1);
    CPPUNIT_TEST(testHealthCheck2);
    CPPUNIT_TEST(testHealthCheck3);
    CPPUNIT_TEST_SUITE_END();

  public:
    class HealthUpdater : public Periodic {
      public:
        HealthUpdater(int64_t msecsFrequency, 
                      const shared_ptr<Notifyable> &notifyableSP, 
                      ClientData *clientData)
            : Periodic(msecsFrequency, notifyableSP, clientData) {}
        
        virtual void run() 
        {
            const shared_ptr<Notifyable> &notifyableSP = getNotifyable();
            if (notifyableSP == NULL) {
                return;
            }

            NotifyableLocker l(notifyableSP,
                               CLString::NOTIFYABLE_LOCK,
                               DIST_LOCK_EXCL);

            notifyableSP->cachedCurrentState().set(
                Node::HEALTH_KEY, getHealth());
            notifyableSP->cachedCurrentState().publish();
        }
        
        void setHealth(const string &health)
        {
            Locker l(&m_lock);
            
            m_health = health;
        }
        
        string getHealth()
        {
            Locker l(&m_lock);
            
            return m_health;
        }
        
        virtual ~HealthUpdater() {}
        
      private:
        Mutex m_lock;
        
        string m_health;
    };

    ClusterlibHealthCheck() 
        : MPITestFixture(globalTestParams),
          _factory(NULL),
          _healthUpdater(10, shared_ptr<Notifyable>(), NULL) {}
    
    /**
     * Runs prior to each test 
     */
    virtual void setUp() 
    {
	_factory = new Factory(
            globalTestParams.getZkServerPortList());
	MPI_CPPUNIT_ASSERT(_factory != NULL);
	_client0 = _factory->createClient();
	MPI_CPPUNIT_ASSERT(_client0 != NULL);
	_app0 = _client0->getRoot()->getApplication(
            appName, CREATE_IF_NOT_FOUND);
	MPI_CPPUNIT_ASSERT(_app0 != NULL);
	_group0 = _app0->getGroup("servers", CREATE_IF_NOT_FOUND);
	MPI_CPPUNIT_ASSERT(_group0 != NULL);
	_node0 = _group0->getNode("server-0", CREATE_IF_NOT_FOUND);
	MPI_CPPUNIT_ASSERT(_node0 != NULL);
    }

    /** 
     * Runs after each test 
     */
    virtual void tearDown() 
    {
        cleanAndBarrierMPITest(_factory, true);
	delete _factory;
        _factory = NULL;
    }

    /** 
     * Simple test to try register a health checker on a node.
     */
    void testHealthCheck1()
    {
        initializeAndBarrierMPITest(1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testHealthCheck1");

        if (isMyRank(0)) {
            MPI_CPPUNIT_ASSERT(_node0->getState() == Notifyable::READY);
            _node0->remove(true);
            MPI_CPPUNIT_ASSERT(_node0->getState() == Notifyable::REMOVED);
            _node0 = _group0->getNode("server-0", CREATE_IF_NOT_FOUND);
            MPI_CPPUNIT_ASSERT(_node0);
            MPI_CPPUNIT_ASSERT(_node0->getState() == Notifyable::READY);
            JSONValue jsonHealth;
            bool found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == false);

            string id;
            int64_t time;
            _node0->acquireLock(CLString::OWNERSHIP_LOCK,
                                DIST_LOCK_EXCL);
            bool hasOwner = _node0->getLockInfo(
                CLString::OWNERSHIP_LOCK, &id, NULL, &time);
            MPI_CPPUNIT_ASSERT(hasOwner == true);
            cerr << "testHealthCheck1: hasOwner=" << hasOwner 
                 << ",id=" << id << ",time=" << time << endl << endl;

            _healthUpdater.setNotifyable(_node0);
            _healthUpdater.setHealth(Node::HEALTH_GOOD_VALUE);
            _factory->registerPeriodicThread(_healthUpdater);
            sleep(1);
            /*
             * Since the time to check is 10 ms, 1 second should be
             * enough time to get the health updated in the cache.
             */            
            found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == true);
            MPI_CPPUNIT_ASSERT(jsonHealth.get<JSONValue::JSONString>() ==
                               Node::HEALTH_GOOD_VALUE);

            _factory->cancelPeriodicThread(_healthUpdater);
            _node0->releaseLock(CLString::OWNERSHIP_LOCK);
        }
    }

    /** 
     * Simple test for to make sure that error cases are handled.
     */
    void testHealthCheck2()
    {
        initializeAndBarrierMPITest(-1, 
                                    true,
                                    _factory, 
                                    true, 
                                    "testHealthCheck2");
        bool cancelled = _factory->cancelPeriodicThread(_healthUpdater);
        MPI_CPPUNIT_ASSERT(cancelled == false);
        _node0->acquireLock(CLString::OWNERSHIP_LOCK,
                            DIST_LOCK_EXCL);
        /* Do nothing */
        _healthUpdater.setNotifyable(shared_ptr<Notifyable>());
        _factory->registerPeriodicThread(_healthUpdater);
        _node0->releaseLock(CLString::OWNERSHIP_LOCK);
        /* Even though cancelPeriodicThread() is not called,
         * things should be cleaned up without any exceptions or
         * memory leaks */
    }

    /** 
     * Test for two processes. One process gets the health checker on
     * the node, then the other will monitor health to see the
     * changes.
     */
    void testHealthCheck3()
    {
        initializeAndBarrierMPITest(2, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testHealthCheck3");

        JSONValue jsonHealth;
        bool found;

        if (isMyRank(0)) {
            MPI_CPPUNIT_ASSERT(_node0->getState() == Notifyable::READY);
            _node0->remove(true);
            MPI_CPPUNIT_ASSERT(_node0->getState() == Notifyable::REMOVED);
            _node0 = _group0->getNode("server-0", CREATE_IF_NOT_FOUND);
            MPI_CPPUNIT_ASSERT(_node0);
            MPI_CPPUNIT_ASSERT(_node0->getState() == Notifyable::READY);
            found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == false);

            _node0->acquireLock(CLString::OWNERSHIP_LOCK,
                                DIST_LOCK_EXCL);
            _healthUpdater.setNotifyable(_node0);
            _healthUpdater.setHealth(Node::HEALTH_GOOD_VALUE);
            _factory->registerPeriodicThread(_healthUpdater);
            sleep(1);
            /*
             * Since the time to check is 10 ms, 1 second should be
             * enough time to get the health updated in the cache.
             */
            found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == true);
            MPI_CPPUNIT_ASSERT(jsonHealth.get<JSONValue::JSONString>() ==
                               Node::HEALTH_GOOD_VALUE);
        }

        waitsForOrder(0, 1, _factory, true);

        if (isMyRank(1)) {
            _node0 = _group0->getNode("server-0", LOAD_FROM_REPOSITORY);
            MPI_CPPUNIT_ASSERT(_node0);

            found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == true);
            MPI_CPPUNIT_ASSERT(jsonHealth.get<JSONValue::JSONString>() ==
                               Node::HEALTH_GOOD_VALUE);
        }
            
        waitsForOrder(1, 0, _factory, true);
            
        if (isMyRank(0)) {
            _healthUpdater.setHealth(Node::HEALTH_BAD_VALUE);
            sleep(1);

            found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == true);
            MPI_CPPUNIT_ASSERT(jsonHealth.get<JSONValue::JSONString>() ==
                               Node::HEALTH_BAD_VALUE);
        }

        waitsForOrder(0, 1, _factory, true);

        if (isMyRank(1)) {
            found = _node0->cachedCurrentState().get(
                Node::HEALTH_KEY, jsonHealth);
            MPI_CPPUNIT_ASSERT(found == true);
            MPI_CPPUNIT_ASSERT(jsonHealth.get<JSONValue::JSONString>() ==
                               Node::HEALTH_BAD_VALUE);
           
        }

        if (isMyRank(0)) {
            _factory->cancelPeriodicThread(_healthUpdater);
            _node0->releaseLock(CLString::OWNERSHIP_LOCK);
        }
   }

  private:
    Factory *_factory;
    Client *_client0;
    shared_ptr<Application> _app0;
    shared_ptr<Group> _group0;
    shared_ptr<Node> _node0;
    HealthUpdater _healthUpdater;
};

/* Registers the fixture into the 'registry' */
CPPUNIT_TEST_SUITE_REGISTRATION(ClusterlibHealthCheck);

