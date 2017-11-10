/***************************************************************************
  tag: The SourceWorks  Tue Sep 7 00:55:18 CEST 2010  TaskContextI.h

                        TaskContextI.h -  description
                           -------------------
    begin                : Tue September 07 2010
    copyright            : (C) 2010 The SourceWorks
    email                : peter@thesourceworks.com

 ***************************************************************************
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public                   *
 *   License as published by the Free Software Foundation;                 *
 *   version 2 of the License.                                             *
 *                                                                         *
 *   As a special exception, you may use this file as part of a free       *
 *   software library without restriction.  Specifically, if other files   *
 *   instantiate templates or use macros or inline functions from this     *
 *   file, or you compile this file and link it with other files to        *
 *   produce an executable, this file does not by itself cause the         *
 *   resulting executable to be covered by the GNU General Public          *
 *   License.  This exception does not however invalidate any other        *
 *   reasons why the executable file might be covered by the GNU General   *
 *   Public License.                                                       *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public             *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place,                                    *
 *   Suite 330, Boston, MA  02111-1307  USA                                *
 *                                                                         *
 ***************************************************************************/


// -*- C++ -*-
//
// $Id$

// ****  Code generated by the The ACE ORB (TAO) IDL Compiler ****
// TAO and the TAO IDL Compiler have been developed by:
//       Center for Distributed Object Computing
//       Washington University
//       St. Louis, MO
//       USA
//       http://www.cs.wustl.edu/~schmidt/doc-center.html
// and
//       Distributed Object Computing Laboratory
//       University of California at Irvine
//       Irvine, CA
//       USA
//       http://doc.ece.uci.edu/
// and
//       Institute for Software Integrated Systems
//       Vanderbilt University
//       Nashville, TN
//       USA
//       http://www.isis.vanderbilt.edu/
//
// Information about TAO is available at:
//     http://www.cs.wustl.edu/~schmidt/TAO.html

// TAO_IDL - Generated from
// ../../../ACE_wrappers/TAO/TAO_IDL/be/be_codegen.cpp:1133

#ifndef ORO_CORBA_TASKCONTEXTI_H_
#define ORO_CORBA_TASKCONTEXTI_H_

#include "corba.h"
#ifdef CORBA_IS_TAO
#include "TaskContextS.h"
#else
#include "TaskContextC.h"
#endif

#include "ServiceC.h"
#include "ServiceRequesterC.h"
#include "DataFlowC.h"
#include "../../TaskContext.hpp"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

class  RTT_corba_CTaskContext_i
  : public virtual POA_RTT::corba::CTaskContext, public virtual PortableServer::RefCountServantBase
{
protected:
    PortableServer::POA_var mpoa;
    RTT::TaskContext* mtask;

    RTT::corba::CService_var mService;
    RTT::corba::CServiceRequester_var mRequest;

    PortableServer::ServantBase_var mRequest_i;
    PortableServer::ServantBase_var mService_i;

public:
  // Constructor
  RTT_corba_CTaskContext_i (RTT::TaskContext* orig, PortableServer::POA_ptr the_poa);

  // Destructor
  virtual ~RTT_corba_CTaskContext_i (void);

  virtual RTT::corba::CTaskContext_ptr activate_this() {
      PortableServer::ObjectId_var oid = mpoa->activate_object(this); // ref count=2
      //_remove_ref(); // ref count=1
      return _this();
  }

  void shutdownCORBA();

  virtual
  char * getName (
      void);

  virtual
  char * getDescription (
      void);

  virtual
  ::RTT::corba::CTaskContextDescription * getCTaskContextDescription (
      void);

  virtual
  ::RTT::corba::CTaskState getTaskState (
      void);

  virtual
  ::CORBA::Boolean configure (
      void);

  virtual
  ::CORBA::Boolean start (
      void);

  virtual
  ::CORBA::Boolean activate (
      void);

  virtual
  ::CORBA::Boolean stop (
      void);

  virtual
  ::CORBA::Boolean cleanup (
      void);

  virtual 
  ::CORBA::Boolean recover(
      void);
  
  virtual
  ::CORBA::Boolean resetException(
      void);

  virtual
  ::CORBA::Boolean isActive (
      void);

  virtual
  ::CORBA::Boolean isRunning (
      void);

  virtual
  ::CORBA::Boolean isConfigured (
      void);

  virtual
  ::CORBA::Boolean inFatalError (
      void);

  virtual
  ::CORBA::Boolean inRunTimeError (
      void);

  virtual
  ::RTT::corba::CDataFlowInterface_ptr ports (
      void);

  virtual
  ::RTT::corba::CService_ptr getProvider (
      const char * service_name);

  virtual
  ::RTT::corba::CServiceRequester_ptr getRequester (
      const char * service_name);

  virtual
  ::RTT::corba::CTaskContext::CPeerNames * getPeerList (
      void);

  virtual
  ::RTT::corba::CTaskContext_ptr getPeer (
      const char * name);

  virtual
  ::CORBA::Boolean hasPeer (
      const char * name);

  virtual
  ::CORBA::Boolean addPeer (
      ::RTT::corba::CTaskContext_ptr p,
      const char * alias);

  virtual
  ::CORBA::Boolean removePeer (
      const char * name);

  virtual
  ::CORBA::Boolean connectPeers (
      ::RTT::corba::CTaskContext_ptr p);

  virtual
  ::CORBA::Boolean disconnectPeers (
      const char * name);

  virtual
  ::CORBA::Boolean connectPorts (
      ::RTT::corba::CTaskContext_ptr p);

  virtual
  ::CORBA::Boolean connectServices (
      ::RTT::corba::CTaskContext_ptr p);
};


#endif /* TASKCONTEXTI_H_  */

