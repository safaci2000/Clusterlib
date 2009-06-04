/*
 * group.h --
 *
 * Interface class Group; it represents a set of nodes within a
 * specific application of clusterlib.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */

#ifndef	_GROUP_H_
#define _GROUP_H_

namespace clusterlib
{

/**
 * Definition of class Group.
 */
class Group
    : public virtual Notifyable
{
  public:
    /**
     * Become the leader of this group.
     */
    virtual void becomeLeader() = 0;

    /**
     * Relinquish leadership of this group.
     */
    virtual void abdicateLeader() = 0;

    /**
     * Check if I am the leader of this group
     */
    virtual bool isLeader() = 0;

    /**
     * Get a list of names of all nodes.
     * 
     * @return a copy of the list of all nodes
     */
    virtual NameList getNodeNames() = 0;

    /**
     * Get the named node.
     * 
     * @param create create the node if doesn't exist
     * @return NULL if the named node does not exist and create
     * == false
     * @throw Exception only if tried to create and couldn't create
     */
    virtual Node *getNode(const std::string &nodeName, 
                          bool create = false) = 0;

    /**
     * Get a list of names of all groups.
     * 
     * @return a copy of the list of all groups
     */
    virtual NameList getGroupNames() = 0;

    /**
     * Get the named group.
     * 
     * @param create create the group if doesn't exist.
     * @return NULL if the group does not exist and create
     * == false, else the Group *.
     * @throw Exception only if tried to create and couldn't create
     */
    virtual Group *getGroup(const std::string &groupName, 
                            bool create = false) = 0;


    /**
     * Get a list of names of all data distributions.
     * 
     * @return a copy of the list of all data distributions
     */
    virtual NameList getDataDistributionNames() = 0;

    /**
     * Get the named distribution.
     * 
     * @param create create the distribution if doesn't exist
     * @return NULL if no distribution exists for this notifyable
     * @throw Exception only if tried to create and couldn't create
     */
    virtual DataDistribution *getDataDistribution(const std::string &distName,
                                                  bool create = false) = 0;

    /*
     * Destructor.
     */
    virtual ~Group() {};
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_GROUP_H_ */