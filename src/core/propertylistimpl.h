/*
 * propertylistimpl.h --
 *
 * Definition of class PropertyListImpl; it represents the property list of a
 * clusterlib object.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */


#ifndef _PROPERTYLISTIMPL_H__
#define _PROPERTYLISTIMPL_H__

namespace clusterlib
{

/**
 * Definition of class PropertyListImpl
 */
class PropertyListImpl
    : public virtual PropertyList,
      public virtual NotifyableImpl
{
    public:
    virtual std::vector<std::string> getPropertyListKeys() const;
         
    virtual std::string getProperty(const std::string &name, 
                                    bool searchParent = false);
        
    virtual void setProperty(const std::string &name, 
                             const std::string &value);

    virtual void deleteProperty(const std::string &name);

    virtual void publish();

    virtual void reset() 
    {
        Locker l1(getSyncLock());
	m_keyValMap.clear();
    }

    virtual int64_t getValueChangeTime() { return m_valueChangeTime; }

    virtual PropertyList *getPropertyList(
        const std::string &name,
        bool create = false);

    /*
     * Internal functions not used by outside clients
     */    
  public:
    /*
     * Constructor used by the factory.
     */
    PropertyListImpl(FactoryOps *fp,
                     const std::string &key,
                     const std::string &name,
                     NotifyableImpl *parent)
        : NotifyableImpl(fp, key, name, parent),
          m_keyValMapVersion(-2),
          m_valueChangeTime(0) {}
    
    virtual void initializeCachedRepresentation();

    virtual void removeRepositoryEntries();

    /**
     * \brief Update the property list map from the repository.
     */
    void updatePropertyListMap();

    /**
     * \brief Converts this property list map into a string.
     * 
     * @return the string representation of this object
     */
    std::string marshall() const;
        
    /**
     * \brief Unmarshalls the given property list into this object.
     * This method will effectively override existing
     * properties unless there is a parse error.
     * 
     * @param marshalledPropertyListString the marshalled up property list
     * @return whether the method successfully unmarshalled the property 
     *         list or not
     */
    bool unmarshall(const std::string &marshalledPropertyListString);

    /*
     * Retrieve the current version # of the property list.
     */
    int32_t getKeyValVersion() { return m_keyValMapVersion; }

    /*
     * Set the version #.
     */
    void setKeyValVersion(int32_t version) { m_keyValMapVersion = version; }

    /*
     * Set the time at which the value changed.
     */
    void setValueChangeTime(int64_t t) { m_valueChangeTime = t; }

  private:
    /*
     * The default constructor is private so no one can call it.
     */
    PropertyListImpl()
        : NotifyableImpl(NULL, "", "", NULL)
    {
        throw InvalidMethodException("Someone called the PropertyListImpl "
                                       "default constructor!");
    }


  private:
    /**             
     * The maps of properties.   
     */
    KeyValMap m_keyValMap;
    int32_t m_keyValMapVersion;

    /*
     * The time of the last change in value.
     */
    int64_t m_valueChangeTime;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_PROPERTYLISTIMPL_H_ */
