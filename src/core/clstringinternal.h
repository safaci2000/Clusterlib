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

#ifndef	_CL_CLSTRINGINTERNAL_H_
#define	_CL_CLSTRINGINTERNAL_H_

namespace clusterlib {

/**
 * All internal available static string variables are declared here.
 */
class CLStringInternal {
  public:
    /**
     * Only root name
     */
    const static std::string ROOT_NAME;

    /**
     * Zookeeper root node
     * (internal)
     */
    const static std::string ROOT_ZNODE;
    /**
     * Denotes the first path after the ROOT_ZNODE
     * (internal)
     */
    const static std::string CLUSTERLIB;

    /**
     * Version of this library being used.
     * (internal)
     */
    const static std::string CLUSTERLIB_VERSION;

    /**
     * Denotes a special sync event for clusterlib to get up-to-date
     * (internal)
     */
    const static std::string SYNC;

    /**
     * Used to generate the CachedCurrentState znode for Notifyable.
     * (internal)
     */
    const static std::string CURRENT_STATE_JSON_VALUE;

    /**
     * Used to generate the CachedDesiredState znode for Notifyable.
     * (internal)
     */
    const static std::string DESIRED_STATE_JSON_VALUE;

    /**
     * Used to generate the queue parent znode for a Queue.
     * (internal)
     */
    const static std::string QUEUE_PARENT;

    /**
     * Queue uses this prefix for its elements.
     * (internal)
     */
    const static std::string QUEUE_ELEMENT_PREFIX;

    /**
     * Used to generate a JSON Object for ProcessSlot objects
     * (internal) for a Node.
     */
    const static std::string PROCESSSLOT_INFO_JSON_OBJECT;

    /**
     * Used to create generic JSON object znodes
     * (internal)
     */
    const static std::string DEFAULT_JSON_OBJECT;

    /**
     * Used to create the keyval JSON object znode for PropertyList
     * (internal)
     */
    const static std::string KEYVAL_JSON_OBJECT;

    /**
     * Used to create the CachedProcessSlotInfo JSON object znode for
     * ProcessSlot. (internal)
     */
    const static std::string PROCESSINFO_JSON_OBJECT;

    /**
     * Used to create the CachedShards JSON object znode for
     * DataDistribution. (internal)
     */
    const static std::string SHARD_JSON_OBJECT;

    /**
     * Denotes the Zookeeper sequence znode splitter.
     * (internal)
     */
    const static std::string SEQUENCE_SPLIT;

    /**
     * Directory of locks in DistributedLocks.
     * (internal)
     */
    const static std::string LOCK_DIR;

    /**
     * Directory of Barrier objects (not implemented).
     * (internal)
     */
    const static std::string BARRIER_DIR;

    /**
     * Directory of Transaction objects (not implemented).
     * (internal)
     */
    const static std::string TRANSACTION_DIR;

    /**
     * Special znode path that denotes that everything should be shutdown.
     * (internal)
     */
    const static std::string END_EVENT;

    /**
     * Used to detect whether the ZooKeeper node is part of a lock.
     * (internal)
     */
    const static std::string PARTIAL_LOCK_NODE;

  private:
    /**
     * No constructing.
     */
    CLStringInternal();
};

}	/* End of 'namespace clusterlib' */

#endif	/* !_CL_CLSTRINGINTERNAL_H_ */
