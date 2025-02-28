/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleServerComponent_h
#define MuscleServerComponent_h

#include "message/Message.h"
#include "support/NotCopyable.h"
#include "util/IPAddress.h"
#include "util/PulseNode.h"
#include "util/RefCount.h"

namespace muscle {

class AbstractReflectSession;
class ReflectServer;
class ReflectSessionFactory;

DECLARE_REFTYPES(AbstractReflectSession);
DECLARE_REFTYPES(ReflectSessionFactory);

#ifndef MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS
/** Defines the maximum amount of time (in microseconds) that an asynchronous TCP connection should be allowed to remain in the "still-connecting" state
  * before the MUSCLE TCP async-connect logic decides it is taking too long and aborts the connection.  Default value is MUSCLE_TIME_NEVER, meaning that
  * MUSCLE will not enforce any explicit time limit (rather only the timeouts implemented by the operating system's network stack will apply).  This value
  * can be overridden at compile-time via e.g. -DMUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS=MinutesToMicros(2), or you can specify an explicit
  * per-connection timeout value at runtime, as an argument to your AddNewConnectSession() calls, etc.
  */
# define MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS MUSCLE_TIME_NEVER
#endif

/**
 *  This class represents any object that can be added to a ReflectServer object
 *  in one way or another, to help define the ReflectServer's behaviour.  This
 *  class provides callback wrappers that let you operate on the server's state.
 */
class ServerComponent : public RefCountable, public PulseNode, private NotCopyable
{
public:
   /** Default Constructor. */
   ServerComponent();

   /** Destructor. */
   virtual ~ServerComponent();

   /** Returns a pretty/human-readable string identifying this class.
     * Default implementation generates a type-name from our subclass's
     * RTTI info; subclasses may override this to return something prettier,
     * if they wish.
     */
   virtual const char * GetTypeName() const;

   /**
    * This method is called when this object has been added to
    * a ReflectServer object.  When this method is called, it
    * is okay to call the other methods in the ServerComponent API.
    * Should return B_NO_ERROR if everything is okay; something
    * else if there is a problem and the attachment should be aborted.
    * Default implementation does nothing and returns B_NO_ERROR.
    * If you override this, be sure to call your superclass's implementation
    * of this method as the first thing in your implementation, and
    * if it doesn't return B_NO_ERROR, immediately return an error yourself.
    */
   virtual status_t AttachedToServer();

   /**
    * This method is called just before we are removed from the
    * ReflectServer object.  Methods in the ServerComponent API may
    * still be called at this time (but not after this method returns).
    * Default implementation does nothing.
    * If you override this, be sure to call you superclass's implementation
    * of this method as the last thing you do in your implementation.
    */
   virtual void AboutToDetachFromServer();

   /**
    * Called when a message is sent to us by an AbstractReflectSession object.
    * Default implementation is a no-op.
    * @param from The session who is sending the Message to us.
    * @param msg A reference to the message that was sent.
    * @param userData Additional data whose semantics are determined by the sending subclass.
    *                 (For StorageReflectSessions, this value, if non-NULL, is a pointer to the
    *                  DataNode in this Session's node subtree that was matched by the paths in (msg))
    */
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData);

   /**
    * Called when a message is sent to us by a ReflectSessionFactory object.
    * Default implementation is a no-op.
    * @param from The session who is sending the Message to us.
    * @param msg A reference to the message that was sent.
    * @param userData Additional data whose semantics are determined by the sending subclass.
    */
   virtual void MessageReceivedFromFactory(ReflectSessionFactory & from, const MessageRef & msg, void * userData);

   /** Returns true if we are attached to the ReflectServer object, false if we are not.  */
   bool IsAttachedToServer() const {return (_owner != NULL);}

   /** Returns true if we are attached FULLY to the ReflectServer object, false if we are not.
     * The difference between this method and IsAttachedToServer() is that this method only returns
     * true after AttachedToServer() has completed successully, and before AboutToDetachFromServer()
     * has been called.  Compare that to IsAttachedToServer()'s which returns true during the
     * AttachedToServer and AboutToDetachFromServer() calls themselves, also.
     */
   bool IsFullyAttachedToServer() const {return _fullyAttached;}

   /** Sets the fully-attached-to-server flag for this session.  Typically only the ReflectServer class should call this.
     * @param fullyAttached true iff we are not fully attached; false if we are no longer fully attacked
     */
   void SetFullyAttachedToServer(bool fullyAttached) {_fullyAttached = fullyAttached;}

   /** Returns the ReflectServer we are currently attached to, or NULL if we aren't currently attached to a ReflectServer. */
   ReflectServer * GetOwner() const {return _owner;}

   /** Sets the ReflectServer we are currently attached to.  Don't call this if you don't know what you are doing.
     * @param s the ReflectServer object that we should use for method-call forwarding to ReflectServer functionality
     */
   void SetOwner(ReflectServer * s) {_owner = s;}

protected:
   /** Returns the value GetRunTime64() was at when this server's ServerProcessLoop() began. */
   uint64 GetServerStartTime() const;

   /** Returns a number that is unique (hopefully) to our ReflectServer object in this process */
   uint64 GetServerSessionID() const;

   /** Returns the number of bytes that are currently available to be allocated */
   uint64 GetNumAvailableBytes() const;

   /** Returns the maximum number of bytes that may be allocated at any given time */
   uint64 GetMaxNumBytes() const;

   /** Returns the number of bytes that are currently allocated */
   uint64 GetNumUsedBytes() const;

   /** Passes through to ReflectServer::PutAcceptFactory()
    *  @param port The TCP port the server will listen on.  (muscled's traditional port is 2960)
    *              If this port is zero, then the server will choose an available port number to use.
    *              If this port is the same as one specified in a previous call to PutAcceptFactory(),
    *              the old factory associated with that port will be replaced with this one.
    *  @param factoryRef Reference to a factory object that can generate new sessions when needed.
    *  @param optInterfaceIP Optional local interface address to listen on.  If not specified, or if specified
    *                        as (invalidIP), then connections will be accepted from all local network interfaces.
    *  @param optRetPort If specified non-NULL, then on success the port that the factory was bound to will
    *                    be placed into this parameter.
    *  @return B_NO_ERROR on success, or an error code on failure (couldn't bind to socket?)
    */
   status_t PutAcceptFactory(uint16 port, const ReflectSessionFactoryRef & factoryRef, const IPAddress & optInterfaceIP = invalidIP, uint16 * optRetPort = NULL);

   /** Passes through to ReflectServer::RemoveAcceptFactory()
    *  @param port whose callback should be removed.  If (port) is set to zero, all callbacks will be removed.
    *  @param optInterfaceIP Interface(s) that the specified callbacks were assigned to in their PutAcceptFactory() call.
    *                        This parameter is ignored when (port) is zero.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if a factory for the specified port was not found.
    */
   status_t RemoveAcceptFactory(uint16 port, const IPAddress & optInterfaceIP = invalidIP);

   /** Tells the whole server process to quit ASAP.  */
   void EndServer();

   /**
    * Returns a reference to a Message that is shared by all objects in
    * a single ReflectServer.  This message can be used for whatever
    * purpose the ServerComponents care to; it is not used by the
    * server itself.  (Note that StorageReflectSessions add data to
    * this Message and expect it to remain there, so be careful not
    * to remove or overwrite it if you are using StorageReflectSessions)
    */
   Message & GetCentralState() const;

   /**
    * Adds the given AbstractReflectSession to the server's session list.
    * If (socket) is less than zero, no TCP connection will be used,
    * and the session will be a pure server-side entity.
    * @param session A reference to the new session to add to the server.
    * @param socket the socket descriptor associated with the new session, or a NULL reference.
    *               If the socket descriptor argument is a NULL reference, the session's
    *               CreateDefaultSocket() method will be called to supply the ConstSocketRef.
    *               If that also returns a NULL reference, then the client will run without
    *               a connection to anything.
    * @return B_NO_ERROR if the session was successfully added, or an error code on error.
    */
   status_t AddNewSession(const AbstractReflectSessionRef & session, const ConstSocketRef & socket = ConstSocketRef());

   /**
    * Like AddNewSession(), only creates a session that connects asynchronously to
    * the given IP address.  AttachedToServer() will be called immediately on the
    * session, and then when the connection is complete, AsyncConnectCompleted() will
    * be called.  Other than that, however, (session) will behave like any other session,
    * except any I/O messages for the client won't be transferred until the connection
    * completes.
    * @param session A reference to the new session to add to the server.
    * @param targetIPAddressAndPort IP address and port to connect to.
    * @param autoReconnectDelay If specified, this is the number of microseconds after the
    *                           connection is broken that an automatic reconnect should be
    *                           attempted.  If not specified, an automatic reconnect will not
    *                           be attempted, and the session will be removed when the
    *                           connection breaks.  Specifying this is equivalent to calling
    *                           SetAutoReconnectDelay() on (session).
    * @param maxAsyncConnectPeriod If specified, this is the maximum time (in microseconds) that we will
    *                              wait for the asynchronous TCP connection to complete.  If this amount of time passes
    *                              and the TCP connection still has not been established, we will force the connection attempt to
    *                              abort.  If not specified, the default value (as specified by MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
    *                              is used; typically this means that it will be left up to the operating system how long to wait
    *                              before timing out the connection attempt.
    * @return B_NO_ERROR if the session was successfully added, or an error code on error
    *                    (out-of-memory or the connect attempt failed immediately).
    */
   status_t AddNewConnectSession(const AbstractReflectSessionRef & session, const IPAddressAndPort & targetIPAddressAndPort, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS);

   /**
    * Like AddNewConnectSession(), except that the added session will not initiate
    * a TCP connection to the specified address immediately.  Instead, it will just
    * hang out and do nothing until you call Reconnect() on it.  Only then will it
    * create the TCP connection to the address specified here.
    * @param ref New session to add to the server.
    * @param targetIPAddressAndPort IP address and port to connect to
    * @param autoReconnectDelay If specified, this is the number of microseconds after the
    *                           connection is broken that an automatic reconnect should be
    *                           attempted.  If not specified, an automatic reconnect will not
    *                           be attempted, and the session will be removed when the
    *                           connection breaks.  Specifying this is equivalent to calling
    *                           SetAutoReconnectDelay() on (ref).
    * @param maxAsyncConnectPeriod If specified, this is the maximum time (in microseconds) that we will
    *                              wait for the asynchronous TCP connection to complete.  If this amount of time passes
    *                              and the TCP connection still has not been established, we will force the connection attempt to
    *                              abort.  If not specified, the default value (as specified by MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
    *                              is used; typically this means that it will be left up to the operating system how long to wait
    *                              before timing out the connection attempt.
    * @return B_NO_ERROR if the session was successfully added, or an error code on error
    *                    (out-of-memory, or the connect attempt failed immediately)
    */
   status_t AddNewDormantConnectSession(const AbstractReflectSessionRef & ref, const IPAddressAndPort & targetIPAddressAndPort, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS);

   /** Returns our server's table of attached sessions. */
   const Hashtable<const String *, AbstractReflectSessionRef> & GetSessions() const;

   /** Returns our server's table of attached sessions, indexed by uint32. */
   const Hashtable<uint32, AbstractReflectSessionRef> & GetSessionsByIDNumber() const;

   /**
    * Looks up a session connected to our ReflectServer via its session ID string.
    * @param id The ID of the session you are looking for.
    * @return A reference to the session with the given session ID, or a NULL reference on failure.
    */
   AbstractReflectSessionRef GetSession(uint32 id) const;

   /**
    * Looks up a session connected to our ReflectServer via its session ID string.
    * @param idStr The ID string of the session you are looking for.
    * @return A reference to the session with the given session ID, or a NULL reference on failure.
    */
   AbstractReflectSessionRef GetSession(const String & idStr) const;

   /** Convenience method:  Returns a pointer to the first session of the specified type.  Returns NULL if no session of the specified type is found.
     * @note this method iterates over the session list, so it's not as efficient as one might hope.
     */
   template <class SessionType> SessionType * FindFirstSessionOfType() const
   {
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         SessionType * ret = dynamic_cast<SessionType *>(iter.GetValue()());
         if (ret) return ret;
      }
      return NULL;
   };

   /** Convenience method:  Populates the specified table with sessions of the specified session type.
     * @param results The list of matching sessions is returned here.
     * @param maxSessionsToReturn No more than this many sessions will be placed into the table.  Defaults to MUSCLE_NO_LIMIT.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   template <class SessionType> status_t FindSessionsOfType(Queue<AbstractReflectSessionRef> & results, uint32 maxSessionsToReturn = MUSCLE_NO_LIMIT) const
   {
      if (maxSessionsToReturn > 0)
      {
         for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
         {
            SessionType * ret = dynamic_cast<SessionType *>(iter.GetValue()());
            if (ret)
            {
               if (results.AddTail(iter.GetValue()).IsError()) return B_OUT_OF_MEMORY;
               if (--maxSessionsToReturn == 0) break;
            }
         }
      }
      return B_NO_ERROR;
   }

   /** Returns the table of session factories currently attached to the server. */
   const Hashtable<IPAddressAndPort, ReflectSessionFactoryRef> & GetFactories() const;

   /** Given a port number, returns a reference to the factory of that port, or a NULL reference if no
such factory exists. */
   ReflectSessionFactoryRef GetFactory(uint16) const;

private:
   ReflectServer * _owner;
   bool _fullyAttached;

   mutable String _rttiTypeName;

   DECLARE_COUNTED_OBJECT(ServerComponent);
};
DECLARE_REFTYPES(ServerComponent);

} // end namespace muscle

#endif
