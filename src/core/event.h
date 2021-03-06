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

#ifndef _CL_EVENT_H_
#define _CL_EVENT_H_

#include <string>
#include <set>
#include <deque>
#include <algorithm>

#include "log.h"
#include "blockingqueue.h"
#include "mutex.h"
#include "thread.h"

DEFINE_LOGGER(EV_LOG, "event")

namespace clusterlib {

/**********************************************************************/
/*                                                                    */
/* INTERNALS OF EVENT SYSTEM -- NOT INTENDED FOR USE BY CLIENTS       */
/*                                                                    */
/**********************************************************************/

/**
 * Enum of the various types of events supported:
 */
enum EventTypes {
    ILLEGALEVENT = -1,
    TIMEREVENT = 0,
    ZKEVENT = 1
};

/*
 * Forward declaration of EventSource.
 */
template<typename E>
class EventSource;

/**
 * \brief This interface is implemented by an observer
 * \brief of a particular {@link EventSource}.
 */
template<typename E>
class EventListener
{
  public:
    /**
     * \brief This method is invoked whenever an event 
     * \brief has been received by the event source being observed.
     * 
     * @param source the source the triggered the event
     * @param event the actual event being triggered
     */
    virtual void eventReceived(const EventSource<E> &source, 
                               const E &e) = 0;

    /**
     * Virtual destructor
     */
    virtual ~EventListener() {}
};  

/**
 * \brief This class represents a source of events.
 * 
 * <p>
 * Each source can have many observers (listeners) attached to it
 * and in case of an event, this source may propagate the event
 * using {@link #fireEvent} method.
 */
template<typename E>           
class EventSource
{
  public:
    /**
     * \brief The type corresponding to the list of registered
     * event listeners.
     */
    typedef std::set<EventListener<E> *> EventListeners;
        
    /**
     * \brief Registers a new event listener.
     * 
     * @param listener the listener to be added to the set of listeners
     */
    void addListener(EventListener<E> *listener)
    {
        Locker l(&m_listenersLock);
        m_listeners.insert(listener);
    }
        
    /**
     * \brief Removes an already registered listener.
     * 
     * @param listener the listener to be removed
     */
    void removeListener(EventListener<E> *listener)
    {
        Locker l(&m_listenersLock);
        m_listeners.erase(listener);
    }
        
    /**
     * \brief Destructor.
     */
    virtual ~EventSource() {}
        
  protected:
    /**
     * \brief Fires the given event to all registered listeners.
     * 
     * <p>
     * This method essentially iterates over all listeners
     * and invokes 
     * {@link fireEvent(EventListener<E> *listener, const E &event)}
     * for each element. All derived classes are free to
     * override the method to provide better error handling
     * than the default implementation.
     * 
     * @param event the event to be propagated to all listeners
     */
    void fireEventToAllListeners(const E &event);
        
    /**
     * \brief Sends an event to the given listener.
     * 
     * @param listener the listener to whom pass the event
     * @param event the event to be handled
     */
    virtual void fireEvent(EventListener<E> *listener, const E &event);
        
  private:
    /**
     * The set of registered event listeners.
     */
    EventListeners m_listeners;
    
    /**
     * Make modifying m_listeners thread-safe
     */
    Mutex m_listenersLock;
};

/**
 * \brief The interface of a generic event wrapper.
 */
class AbstractEventWrapper
{
  public:
        
    /**
     * \brief Destructor.
     */
    virtual ~AbstractEventWrapper() {}
        
    /**
     * \brief Returns the underlying wrapee's data.
     */
    virtual void *getWrapee() = 0;

    /**
     * \brief Clone functionality for deep copy.
     */
    virtual AbstractEventWrapper *clone() const = 0;
};

/**
 * \brief A template based implementation of {@link AbstractEventWrapper}.
 */
template<typename E>
class EventWrapper
    : public AbstractEventWrapper
{
  public:
    EventWrapper(const E &e)
        : m_e(e)
    {
    }
    ~EventWrapper() {}
    void *getWrapee()
    {
        return &m_e;
    }
    virtual AbstractEventWrapper *clone() const {
        return new EventWrapper(m_e);
    }
  private:
    E m_e;
};

/**
 * \brief This class represents a generic event.
 */
class GenericEvent
{
  public:
    /**
     * \brief Constructor.
     */
    GenericEvent() : m_type(0), mp_eventWrapper(NULL) {}

    /**
     * \brief Constructor.
     * 
     * @param type the type of this event
     * @param eventWrapper the wrapper around event's data 
     *                     (transfers ownership to this object)
     */
    GenericEvent(int32_t type, AbstractEventWrapper *eventWrapper)
        : m_type(type),
          mp_eventWrapper(eventWrapper)
    {
    }
        
    ~GenericEvent()
    {
        delete mp_eventWrapper;
    }

    GenericEvent(const GenericEvent &ge)
    {
        m_type = ge.getType();
        mp_eventWrapper = ge.mp_eventWrapper->clone();
    }

    GenericEvent &operator = (const GenericEvent &ge)
    {
        m_type = ge.getType();
        mp_eventWrapper = ge.mp_eventWrapper->clone();
        return *this;
    }

    /**
     * \brief Returns the type of this event.
     * 
     * @return type of this event
     */
    int32_t getType() const { return m_type; }

    static std::string getTypeString(int32_t type)
    {
        std::string res;

        if (type == ILLEGALEVENT) {
            res = "ILLEGALEVENT";
        }
        else if (type == TIMEREVENT) {
            res = "TIMEREVENT";
        }
        else if (type == ZKEVENT) {
            res = "ZKEVENT";
        }
        else {
            res = "unknown type";
        }

        return res;
    }
        
    /**
     * \brief Returns the event's data.
     * 
     * @return the event's data
     */
    void *getEvent() const { return mp_eventWrapper->getWrapee(); }
        
  private:
    /**
     * The event type.
     */
    int32_t m_type;

    /**
     * The event represented as abstract wrapper.
     */
    AbstractEventWrapper *mp_eventWrapper;
};
    
/**
 * \brief This class adapts {@link EventListener} to a generic listener.
 * Essentially this class listens on incoming events and fires them 
 * as {@link GenericEvent}s.
 */
template<typename E, const int32_t type>
class EventListenerAdapter
    : public virtual EventListener<E>,
      public virtual EventSource<GenericEvent>
{
  public:        
    /**
     * \brief Constructor.
     * 
     * @param eventSource the source on which register this listener
     */
    EventListenerAdapter(EventSource<E> &eventSource)
    {
        eventSource.addListener(this);
    }
        
    virtual void eventReceived(const EventSource<E> &source, const E &e)
    {
        LOG_DEBUG(EV_LOG, "EventListenerAdapter::eventReceived: before fire");
        AbstractEventWrapper *wrapper = new EventWrapper<E>(e);
        GenericEvent event(type, wrapper);
        fireEventToAllListeners(event);
    }
};        

/**
 * \brief This class provides an adapter between an asynchronous
 * and synchronous event handling.
 * 
 * <p>
 * This class queues up all received events and exposes them through 
 * {@link #getNextEvent()} method.
 */
template<typename E>                  
class SynchronousEventAdapter
    : public EventListener<E>
{
  public:
    virtual void eventReceived(const EventSource<E> &source, const E &e)
    {
        TRACE(EV_LOG, "eventReceived");
        LOG_DEBUG(EV_LOG, 
                  "eventReceived: event %p, instance %p, thread %" PRId32,
                  &e, 
                  this, 
                  ProcessThreadService::getTid());
        
        m_queue.put(e);
    }

    /**
     * \brief Returns the next available event from the underlying
     * queue, blocking if no data is available.
     * 
     * @param e the element filled in if returned true
     */
    void getNextEvent(E &e)
    {
        TRACE(EV_LOG, "getNextEvent");

        if (!getNextEventWaitMsecs(-1LL, e)) {
            throw InconsistentInternalStateException(
                "getNextEvent: getNextEventWaitMsecs cannot return false!");
        }
    }

    /**
     * \brief Returns the next available event from the underlying
     * queue, possibly blocking, if no data is available.
     * 
     * @param msecTimeout the amount of msecs to wait until giving up, 
     *        -1 means wait forever, 0 means return immediately
     * @param e the element filled in if returned true
     * @return true if an element was retrieved, false otherwise
     */
    bool getNextEventWaitMsecs(int64_t msecTimeout, E &e)
    {
        TRACE(EV_LOG, "getNextEventWaitMsecs");

        LOG_DEBUG(EV_LOG, 
                  "getNextEvent: msecTimeout %" PRId64
                  ", instance %p, thread %" PRId32,
                  msecTimeout,
                  this, 
                  ProcessThreadService::getTid());
        return m_queue.takeWaitMsecs(msecTimeout, e);
    }
        
    /**
     * \brief Returns whether there are any events in the queue or not.
     * 
     * @return true if there is at least one event and 
     *         the next call to {@link #getNextEvent} won't block
     */
    bool hasEvents() const
    {
        return (m_queue.empty() ? false : true);
    }
        
    /**
     * \brief Destructor.
     */
    virtual ~SynchronousEventAdapter() {}

  private:
    /**
     * The blocking queue of all events received so far.
     */
    BlockingQueue<E> m_queue;
};

template<typename E>
void EventSource<E>::fireEventToAllListeners(const E &event)
{
    Locker l(&m_listenersLock);
    for (typename EventListeners::iterator eIt = m_listeners.begin(); 
         eIt != m_listeners.end(); 
         ++eIt) 
    {
        fireEvent(*eIt, event);
    }
}

template<typename E>
void EventSource<E>::fireEvent(EventListener<E> *lp, const E &event)
{
    LOG_DEBUG(EV_LOG,
              "fireEvent: Sending event: event %p, listener %p, "
              "thread %" PRId32, 
              &event, 
              lp, 
              ProcessThreadService::getTid());

    assert(lp);
    lp->eventReceived(*this, event);
}

/*
 * This is a helper class for handling externally visible clusterlib
 * events using a member function.
 */
template<class T>
class NotifyableEventHandler
{
  public:
    /**
     * Define the type of the member function to invoke.
     */
    typedef Event (T::*EventMethod)(
        const boost::shared_ptr<NotifyableImpl> &notifyableSP,
        int32_t etype,
        const std::string &path);

    /**
     * Constructor.
     */
    NotifyableEventHandler(T *objp, EventMethod handler)
        : mp_obj(objp),    
          m_handler(handler) {}

    /**
     * Deliver the event.
     */
    Event deliver(const boost::shared_ptr<NotifyableImpl> &notifyableSP, 
                  int32_t etype,
                  const std::string &path)
    {
        return ((*mp_obj).*m_handler)(notifyableSP, etype, path);
    }

    /**
     * Retrieve the object on which the method
     * is being called.
     */
    T *getObject() { return mp_obj; }

  private:
    /**
     * The instance.
     */
    T *mp_obj;

    /**
     * The handler method to call.
     */
    EventMethod m_handler;
};

/*
 * Externally visible events are delivered to a
 * CachedObjectChangeHandlers object.
 */
typedef NotifyableEventHandler<CachedObjectChangeHandlers> 
CachedObjectEventHandler;

/*
 * This is a helper class for handling internal clusterlib events
 * using a member function.
 */
template<class T>
class EventHandler
{
  public:
    /*
     * Define the type of the member function to invoke.
     */
    typedef Event (T::*EventMethod)(int32_t etype,
                                    const std::string &path);

    /*
     * Constructor.
     */
    EventHandler(T *objp, EventMethod handler)
        : mp_obj(objp),    
          m_handler(handler) {}

    /*
     * Deliver the event.
     */
    Event deliver(int32_t etype,
                  const std::string &path)
    {
        return ((*mp_obj).*m_handler)(etype, path);
    }

    /*
     * Retrieve the object on which the method
     * is being called.
     */
    T *getObject() { return mp_obj; }

  private:
    /*
     * The instance.
     */
    T *mp_obj;

    /*
     * The handler method to call.
     */
    EventMethod m_handler;
};

/*
 * Internal events are delivered to a InternalChangeHandler
 * object.
 */
typedef EventHandler<InternalChangeHandlers> InternalEventHandler;

/**
 * Payload for delivering events from ZooKeeper to clients of
 * Clusterlib.
 */
class UserEventPayload
{
  public:
    /**
     * Constructor.
     */
    UserEventPayload(const std::string &key, Event e)
        : m_key(key),
          m_e(e) {}

    /**
     * Destructor.
     */
    virtual ~UserEventPayload() {}

    /**
     * Retrieve fields.
     */
    Event getEvent() { return m_e; }
    const std::string &getKey() { return m_key; }

  private:
    /**
     * The target path that clients are being notified about.
     */
    std::string m_key;

    /**
     * The event that clients are being notified about.
     */
    Event m_e;
};

/*
 * Typedef for blocking queue of pointers to cluster event payload objects.
 */
typedef BlockingQueue<UserEventPayload *> UserEventPayloadQueue;

/***********************************************************************/
/*                                                                     */
/* Below are user level event handling definitions.                    */
/*                                                                     */
/***********************************************************************/

/*---------------------------------------------------------------------*/
/*                                                                     */
/* PART A: CLUSTER EVENTS                                              */
/*                                                                     */
/*---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*/
/*                                                                     */
/* PART B: TIMER EVENTS                                                */
/*                                                                     */
/*---------------------------------------------------------------------*/

/**
 * This class represents a timer event parametrized by the user's data type.
 */
template<typename T>
class TimerEvent
{
  public:
    /**
     * \brief Constructor.
     * 
     * @param id the ID of this event
     * @param alarmTime when this event is to be triggered
     * @param userData the user data associated with this event
     */
    TimerEvent(TimerId id, int64_t alarmTime, const T &userData)
        : m_id(id),
          m_alarmTime(alarmTime),
          m_userData(userData) 
    {
        LOG_DEBUG(EV_LOG, 
                  "Created timer event: instance %p, "
                  "id %d alarm time %" PRId64, 
                  this, 
                  id, 
                  alarmTime);
    }

    /**
     * \brief Constructor.
     */
    TimerEvent()
        : m_id(-1),
          m_alarmTime(-1)
    {
    }
                           
    /**
     * \brief Returns the ID.
     * 
     * @return the ID of this event
     */
    TimerId getID() const { return m_id; }
        
    /**
     * \brief Returns the alarm time.
     * 
     * @return the alarm time
     */
    int64_t getAlarmTime() const { return m_alarmTime; }
              
    /**
     * \brief Returns the user's data.
     * 
     * @return the user's data
     */
    T const &getUserData() const { return m_userData; }
        
    /**
     * \brief Returns whether the given alarm time is less than this event's 
     * \brief time.
     */
    bool operator<(const int64_t alarmTime) const
    {
        return m_alarmTime < alarmTime;
    }
        
  private:
  
    /**
     * The ID of ths event.
     */
    TimerId m_id;
        
    /**
     * The time at which this event triggers.
     */
    int64_t m_alarmTime;    
        
    /**
     * The user specific data associated with this event.
     */
    T m_userData;
};

template<typename T>
class Timer
    : public EventSource<TimerEvent<T> >
{
  public:
    /**
     * \brief Constructor.
     */
    Timer()
        : m_currentEventID(0),
          m_terminating(false)
    {
        m_workerThread.Create(*this, &Timer<T>::sendAlarms);
    }
        
    /**
     * \brief Destructor.
     */
    virtual ~Timer()
    {
        m_terminating = true;
        m_lock.notify();
        m_workerThread.Join();
    }
        
    /**
     * \brief Schedules the given event <code>timeFromNow</code> milliseconds.
     * 
     * @param timeFromNow time from now, in milliseconds, when the event 
     *                    should be triggered 
     * @param userData the user data associated with the timer event
     * 
     * @return the ID of the newly created timer event
     */
    TimerId scheduleAfter(int64_t timeFromNow, const T &userData)
    {
        return scheduleAt(TimerService::getCurrentTimeMsecs() + timeFromNow, 
                          userData);
    }

    /**
     * \brief Schedules an event at the given time.
     * 
     * @param absTime absolute time, in milliseconds, at which the event 
     *                should be triggered; the time is measured
     *                from Jan 1st, 1970   
     * @param userData the user data associated with the timer event
     * 
     * @return the ID of the newly created timer event
     */
    TimerId scheduleAt(int64_t absTime, const T &userData)
    {
        m_lock.lock();
        typename QueueType::iterator pos = 
            lower_bound(m_queue.begin(), m_queue.end(), absTime);
        TimerId id = m_currentEventID++;
        TimerEvent<T> event(id, absTime, userData); 
        m_queue.insert(pos, event);
        m_lock.notify();
        m_lock.unlock();
        return id;
    }
        
    /**
     * \brief Cancels the given timer event.
     * 
     * @param eventID the ID of the event to be canceled
     * 
     * @return whether the event has been canceled
     */
    bool cancelAlarm(TimerId eventID)
    {
        bool canceled = false;                      
        m_lock.lock();
        typename QueueType::iterator i;
        for (i = m_queue.begin(); i != m_queue.end(); ++i) {
            if (eventID == i->getID()) {
                m_queue.erase(i);
                canceled = true;
                break;
            }
        }
        m_lock.unlock();
        return canceled;
    }
        
    /**
     * Executes the main loop of the worker thread.
     */
    void sendAlarms(void *param)
    {
        LOG_DEBUG(EV_LOG, 
                  "Starting thread with Timer::sendAlarms(), "
                  "this: %p, thread %" PRId32,
                  this, 
                  ProcessThreadService::getTid());
        
        /** Iterate until terminating */
        while (!m_terminating) {
            m_lock.lock();
            //1 step - wait until there is an event in the queue
            if (m_queue.empty()) {
                //wait up to 100ms to get next event
                m_lock.waitMsecs(100);
            }
            bool fire = false;
            if (!m_queue.empty()) {
                //retrieve the event from the queue and send it
                TimerEvent<T> event = m_queue.front();      
                //check whether we can send it right away
                int64_t timeToWait = 
                    event.getAlarmTime() - TimerService::getCurrentTimeMsecs();
                if (timeToWait <= 0) {
                    m_queue.pop_front();
                    //we fire only if it's still in the queue and alarm
                    //time has just elapsed (in case the top event
                    //is canceled)
                    fire = true;    
                } else {
                    m_lock.waitMsecs(timeToWait);
                }
                m_lock.unlock();
                if (fire) {
                    fireEventToAllListeners(event);
                }
            } else {
                m_lock.unlock();
            }
        }

        LOG_DEBUG(EV_LOG,
                  "Ending thread with Timer::sendAlarms(): "
                  "this: %p, thread: %" PRId32,
                  this,
                  ProcessThreadService::getTid());
    }
        
  private:
    /**
     * The type of timer events queue.
     */
    typedef std::deque<TimerEvent<T> > QueueType;
        
    /**
     * The current event ID, auto-incremented each time a new event 
     * is created.
     */
    TimerId m_currentEventID;
        
    /**
     * The queue of timer events sorted by {@link TimerEvent#alarmTime}.
     */
    QueueType m_queue;
        
    /**
     * The lock used to guard {@link #m_queue}.
     */
    Lock m_lock;
        
    /**
     * The thread that triggers alarms.
     */
    CXXThread<Timer<T> > m_workerThread;
        
    /**
     * Whether {@link #m_workerThread}  is terminating.
     */
    volatile bool m_terminating;
};

/*
 * The payload for a timer event.
 */
class TimerEventPayload
{
  public:
    /*
     * Constructor.
     */
    TimerEventPayload(int64_t ending,
                      TimerEventHandler *handler,
                      ClientData data)
        : m_ending(ending),
          mp_handler(handler),
          mp_data(data),
          m_id((TimerId) 0),
          m_cancelled(false)
    {
    };

    /*
     * Destructor.
     */
    virtual ~TimerEventPayload() {}

    /*
     * Retrieve the fields.
     */
    int64_t getEnding() { return m_ending; }
    TimerEventHandler *getHandler() { return mp_handler; }
    ClientData getData() { return mp_data; }
    TimerId getId() { return m_id; }
    bool cancelled() { return m_cancelled; }

    /*
     * Cancel the event.
     */
    void cancel() { m_cancelled = true; }

    /*
     * Update the timer ID.
     */
    void updateTimerId(TimerId id) { m_id = id; }

  private:
    /*
     * When is the timer ending?
     */
    int64_t m_ending;

    /*
     * The event handler itself.
     */
    TimerEventHandler *mp_handler;

    /*
     * Client data associated with this event.
     */
    ClientData mp_data;

    /*
     * Timer ID for this timer.
     */
    TimerId m_id;

    /*
     * Is this timer cancelled?
     */
    bool m_cancelled;
};

/**
 * The types for timer events and timer event source.
 */
typedef TimerEvent<TimerEventPayload *>		ClusterlibTimerEvent;
typedef Timer<TimerEventPayload *>		ClusterlibTimerEventSource;
typedef BlockingQueue<TimerEventPayload *>	TimerEventQueue;

}   /* end of 'namespace clusterlib' */

#endif /* _CL_EVENT_H_ */
