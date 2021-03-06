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

const string appName = "unittests-cache-app";

/**
 * The test class itself.
 *
 * Note: Since these tests violate the clusterlib API, they may cause
 * occasional non-fatal problems.
 */
class ClusterlibCache
    : public MPITestFixture
{
    CPPUNIT_TEST_SUITE(ClusterlibCache);
    CPPUNIT_TEST(testCache1);
    CPPUNIT_TEST(testCache2);
    CPPUNIT_TEST(testCache3);
    CPPUNIT_TEST(testCache4);
    CPPUNIT_TEST(testCache5);
    CPPUNIT_TEST(testCache6);
    CPPUNIT_TEST(testCache7);
    CPPUNIT_TEST(testCache8);
    CPPUNIT_TEST(testCache9);
    CPPUNIT_TEST_SUITE_END();

  public:
    
    ClusterlibCache()
        : MPITestFixture(globalTestParams),
          _factory(NULL),
          _client0(NULL),
          _zk(NULL) {}

    /* Runs prior to each test */
    virtual void setUp() 
    {
	_factory =
            new Factory(globalTestParams.getZkServerPortList());

	MPI_CPPUNIT_ASSERT(_factory != NULL);
        _zk = _factory->getRepository();
        MPI_CPPUNIT_ASSERT(_zk != NULL);
	_client0 = _factory->createClient();
	MPI_CPPUNIT_ASSERT(_client0 != NULL);
        _app0 = _client0->getRoot()->getApplication(appName, 
                                                    CREATE_IF_NOT_FOUND);
        MPI_CPPUNIT_ASSERT(_app0 != NULL);
        _grp0 = _app0->getGroup("bar-group", CREATE_IF_NOT_FOUND);
        MPI_CPPUNIT_ASSERT(_grp0 != NULL);
        _nod0 = _grp0->getNode("nod3", CREATE_IF_NOT_FOUND);
        MPI_CPPUNIT_ASSERT(_nod0 != NULL);
        _dist0 = _grp0->getDataDistribution("dist1", CREATE_IF_NOT_FOUND);
        MPI_CPPUNIT_ASSERT(_dist0 != NULL);
    }

    /* Runs after each test */
    virtual void tearDown() 
    {
        cleanAndBarrierMPITest(_factory, CREATE_IF_NOT_FOUND);
        /*
         * Delete only the factory, that automatically deletes
         * all the other objects.
         */
	delete _factory;
        _factory = NULL;
        _client0 = NULL;
        _app0 = NULL;
        _grp0 = NULL;
        _nod0 = NULL;
        _dist0 = NULL;
    }

    void testCache1()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache1");
        
        /*
         * Set the health report of a node and see if the cache
         * is updated.
         */
        if (!isMyRank(0)) {
            return;
        }

        string hrpath =
            _nod0->getKey() +
            "/" +
            "_clientState";
        _zk->setNodeData(hrpath, "healthy");
        _factory->synchronize();
        string clientState;
        _nod0->getClientState(NULL, &clientState, NULL);
        MPI_CPPUNIT_ASSERT(string("healthy") == clientState);
    }
    void testCache2()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache2");

        /*
         * Test ready protocol.
         *
         * Set and unset the ready bit on a node and see if the
         * cache is updated. Same on group, application, and
         * data distribution.
         */
        if (!isMyRank(0)) {
            return;
        }

        string rpath = _nod0->getKey();
        _zk->setNodeData(rpath, "");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_nod0->getState() == Notifyable::READY);
        _zk->setNodeData(rpath, "ready");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_nod0->getState() == Notifyable::READY);

        rpath = _grp0->getKey();
        _zk->setNodeData(rpath, "");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_grp0->getState() == Notifyable::READY);
        _zk->setNodeData(rpath, "ready");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_grp0->getState() == Notifyable::READY);

        rpath = _app0->getKey();
        _zk->setNodeData(rpath, "");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_app0->getState() == Notifyable::READY);
        _zk->setNodeData(rpath, "ready");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_app0->getState() == Notifyable::READY);

        rpath = _dist0->getKey();
        _zk->setNodeData(rpath, "");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_dist0->getState() == Notifyable::READY);
        _zk->setNodeData(rpath, "ready");
        _factory->synchronize();
        MPI_CPPUNIT_ASSERT(_dist0->getState() == Notifyable::READY);
    }
    void testCache3()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache3");

        /*
         * Testing whether groups change notification works
         * in an application.
         */
        if (!isMyRank(0)) {
            return;
        }

        string rpath =
            _app0->getKey() +
            "/" +
            "_groups" +
            "/" +
            "g15";
        bool found = false;

        /*
         * Make sure the group does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * groups in _app0.
         */
        NameList gnl = _app0->getGroupNames();

        /*
         * Create the new group..
         */
        _app0->getGroup("g15", CREATE_IF_NOT_FOUND);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();
        
        /*
         * Now check that the new group appears.
         */
        gnl = _app0->getGroupNames();
        NameList::iterator nlIt;

        for (nlIt = gnl.begin(); nlIt != gnl.end(); nlIt++) {
            if ((*nlIt) == "g15") {
                found = true;
                break;
            }
        }

        MPI_CPPUNIT_ASSERT(found == true);

        shared_ptr<Group> groupP = _app0->getGroup("g15");

        MPI_CPPUNIT_ASSERT(groupP->getState() == Notifyable::READY);
    }
    void testCache4()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache4");

        /*
         * Testing whether distribution change notification works
         * in an application.
         */
        if (!isMyRank(0)) {
            return;
        }

        string rpath =
            _app0->getKey() +
            "/" +
            "_distributions" +
            "/" +
            "d15";
        bool found = false;

        /*
         * Make sure the distribution does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * distributions in _app0.
         */
        NameList ddnl = _app0->getDataDistributionNames();

        /*
         * Create the new data distribution..
         */
        _app0->getDataDistribution("d15", CREATE_IF_NOT_FOUND);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new distribution appears.
         */
        ddnl = _app0->getDataDistributionNames();

        NameList::iterator nlIt;

        for (nlIt = ddnl.begin(); nlIt != ddnl.end(); nlIt++) {
            if ((*nlIt) == "d15") {
                found = true;
                break;
            }
        }

        MPI_CPPUNIT_ASSERT(found == true);

        shared_ptr<DataDistribution> distP = _app0->getDataDistribution("d15");

        MPI_CPPUNIT_ASSERT(distP->getState() == Notifyable::READY);
    }
    void testCache5()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache5");

        /*
         * Test whether node membership change notification works
         * in a group.
         */

        if (!isMyRank(0)) {
            return;
        }

        string rpath = 
            _grp0->getKey() +
            "/" +
            "_nodes" +
            "/" +
            "n111";
        bool found = false;

        /*
         * Make sure the node does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * nodes in _grp0.
         */
        NameList nnl = _grp0->getNodeNames();

        /*
         * Create a new node..
         */
        _grp0->getNode("n111", CREATE_IF_NOT_FOUND);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new node appears.
         */
        nnl = _grp0->getNodeNames();

        NameList::iterator nlIt;

        for (nlIt = nnl.begin(); nlIt != nnl.end(); nlIt++) {
            if ((*nlIt) == "n111") {
                found = true;
                break;
            }
        }

        MPI_CPPUNIT_ASSERT(found == true);

        shared_ptr<Node> np = _grp0->getNode("n111");

        MPI_CPPUNIT_ASSERT(np->getState() == Notifyable::READY);
    }

    void testCache6()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache6");

        /*
         * Testing whether distribution change notification works
         * in a group.
         */
        if (!isMyRank(0)) {
            return;
        }

        string rpath =
            _grp0->getKey() +
            "/" +
            "_distributions" +
            "/" +
            "d15";
        bool found = false;

        /*
         * Make sure the distribution does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * distributions in _grp0.
         */
        NameList ddnl = _grp0->getDataDistributionNames();

        /*
         * Create the new data distribution..
         */
        _grp0->getDataDistribution("d15", CREATE_IF_NOT_FOUND);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new distribution appears.
         */
        ddnl = _grp0->getDataDistributionNames();

        NameList::iterator nlIt;

        for (nlIt = ddnl.begin(); nlIt != ddnl.end(); nlIt++) {
            if ((*nlIt) == "d15") {
                found = true;
                break;
            }
        }

        MPI_CPPUNIT_ASSERT(found == true);

        shared_ptr<DataDistribution> distP = _grp0->getDataDistribution("d15");

        MPI_CPPUNIT_ASSERT(distP->getState() == Notifyable::READY);
    }
    void testCache7()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache7");

        /*
         * Test whether node membership change notification works
         * in an application.
         */
        if (!isMyRank(0)) {
            return;
        }

        string rpath = 
            _app0->getKey() +
            "/" +
            "_nodes" +
            "/" +
            "n111";
        bool found = false;

        /*
         * Make sure the node does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * nodes in _app0.
         */
        NameList nnl = _app0->getNodeNames();

        /*
         * Create a new node..
         */
        _app0->getNode("n111", CREATE_IF_NOT_FOUND);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new node appears.
         */
        nnl = _app0->getNodeNames();

        NameList::iterator nlIt;
        
        for (nlIt = nnl.begin(); nlIt != nnl.end(); nlIt++) {
            if ((*nlIt) == "n111") {
                found = true;
                break;
            }
        }

        MPI_CPPUNIT_ASSERT(found == true);

        shared_ptr<Node> np = _app0->getNode("n111");

        MPI_CPPUNIT_ASSERT(np->getState() == Notifyable::READY);
    }
    void testCache8()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache8");

        /*
         * Testing whether groups change notification works
         * in a group.
         */
        if (!isMyRank(0)) {
            return;
        }

        string rpath =
            _grp0->getKey() +
            "/" +
            "_groups" +
            "/" +
            "g15";
        bool found = false;

        /*
         * Make sure the group does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * groups in _app0.
         */
        NameList gnl = _grp0->getGroupNames();

        /*
         * Create the new group..
         */
        _grp0->getGroup("g15", CREATE_IF_NOT_FOUND);        

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();
        
        /*
         * Now check that the new group appears.
         */
        gnl = _grp0->getGroupNames();

        NameList::iterator nlIt;

        for (nlIt = gnl.begin(); nlIt != gnl.end(); nlIt++) {
            if ((*nlIt) == "g15") {
                found = true;
                break;
            }
        }

        MPI_CPPUNIT_ASSERT(found == true);

        shared_ptr<Group> groupP = _grp0->getGroup("g15");

        MPI_CPPUNIT_ASSERT(groupP->getState() == Notifyable::READY);
    }
    void testCache9()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache9");

        /*
         * Test whether node connectivity notification
         * works.
         */

        if (!isMyRank(0)) {
            return;
        }

        MPI_CPPUNIT_ASSERT(_nod0->isConnected() == false);

        string rpath =
            _nod0->getKey() +
            "/" +
            "_connected";

        /*
         * Create the connectivity znode.
         */
        _zk->createNode(rpath, 
                        "{\"_connectedId\":\"testCache9\",\"_time\":9}", 
                        0, 
                        false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the node is "connected".
         */
        MPI_CPPUNIT_ASSERT(_nod0->isConnected() == true);

        /*
         * Delete the connectivity znode.
         */
        _zk->deleteNode(rpath, true, -1);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the node is not "connected".
         */
        MPI_CPPUNIT_ASSERT(_nod0->isConnected() == false);
    }
    void testCache10()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache10");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache11()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache11");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache12()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache12");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache13()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache13");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache14()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache14");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache15()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache15");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache16()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache16");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache17()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache17");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache18()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache18");

        if (!isMyRank(0)) {
            return;
        }
    }
    void testCache19()
    {
        initializeAndBarrierMPITest(-1, 
                                    true, 
                                    _factory, 
                                    true, 
                                    "testCache19");

        if (!isMyRank(0)) {
            return;
        }
    }

  private:
    Factory *_factory;
    Client *_client0;
    shared_ptr<Application> _app0;
    shared_ptr<Group> _grp0;
    shared_ptr<Node> _nod0;
    shared_ptr<DataDistribution> _dist0;
    zk::ZooKeeperAdapter *_zk;
};

/* Registers the fixture into the 'registry' */
CPPUNIT_TEST_SUITE_REGISTRATION(ClusterlibCache);

