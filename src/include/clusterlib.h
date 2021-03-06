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

#ifndef	_CL_CLUSTERLIB_H_
#define	_CL_CLUSTERLIB_H_

/*
 * The main include file for users of clusterlib.
 */

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/any.hpp>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <iostream>
#include <sstream> 
#include <typeinfo>
#include <limits>

#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "forwarddecls.h"
#include "clstring.h"
#include "clnumeric.h"

#include "clusterlibexceptions.h"
#include "processthreadservice.h"
#include "jsonexceptions.h"
#include "mutex.h"
#include "timerservice.h"
#include "blockingqueue.h"
#include "thread.h"
#include "cacheddata.h"
#include "healthchecker.h"
#include "periodic.h"

#include "json.h"
#include "jsonrpc.h"
#include "clusterlibrpc.h"
#include "clusterlibrpcbulkrequest.h"
#include "genericrpc.h"

#include "cachedstate.h"
#include "cachedkeyvalues.h"
#include "cachedshards.h"
#include "cachedprocessinfo.h"
#include "hashrange.h"
#include "uint64hashrange.h"
#include "notifyable.h"
#include "group.h"
#include "application.h"
#include "cachedprocessslotinfo.h"
#include "node.h"
#include "processslot.h"
#include "queue.h"
#include "shard.h"
#include "datadistribution.h"
#include "propertylist.h"
#include "root.h"
#include "client.h"
#include "factory.h"

#include "zookeeperperiodiccheck.h"
/** The autoconf config.h */
#include "config.h"

#endif	/* !_CL_CLUSTERLIB_H_ */
