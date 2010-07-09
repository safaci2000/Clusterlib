/*
 * root.h --
 *
 * Interface class Root; it represents a set of applications within
 * clusterlib.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */

#ifndef	_CL_ROOT_H_
#define _CL_ROOT_H_

namespace clusterlib
{

/**
 * Definition of class Root.
 */
class Root
    : public virtual Notifyable
{
  public:
    /**
     * Get a list of names of all applications.
     * 
     * @return a copy of the list of all applications
     */
    virtual NameList getApplicationNames() = 0;

    /**
     * Get the named application.
     * 
     * @param appName the name of the application to get
     * @param accessType The mode of access
     * @return NULL if the named application does not exist and create
     *         == false
     * @throw Exception only if tried to create and couldn't create
     */
    virtual Application *getApplication(
        const std::string &appName,
        AccessType accessType = LOAD_FROM_REPOSITORY) = 0;

    /*
     * Destructor.
     */
    virtual ~Root() {};
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_CL_ROOT_H_ */
