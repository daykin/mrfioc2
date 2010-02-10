/**************************************************************************************************
|* $(MRF)/evgBasicSequenceApp/src/devBasicSequence.cpp --  EPICS Device Support for Basic Sequences
|*-------------------------------------------------------------------------------------------------
|* Authors:  Eric Bjorklund (LANSCE)
|* Date:     14 January 2010
|*
|*-------------------------------------------------------------------------------------------------
|* MODIFICATION HISTORY:
|* 14 Jan 2010  E.Bjorklund     Original
|*
|*-------------------------------------------------------------------------------------------------
|* MODULE DESCRIPTION:
|*    This module contains EPICS device support for event generator Basic Sequence Event objects.
|*
\**************************************************************************************************/

/**************************************************************************************************
|*                                     COPYRIGHT NOTIFICATION
|**************************************************************************************************
|*
|* THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
|* AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
|* AND IN ALL SOURCE LISTINGS OF THE CODE.
|*
|**************************************************************************************************
|*
|* This software is distributed under the EPICS Open License Agreement which
|* can be found in the file, LICENSE, included with this distribution.
|*
\*************************************************************************************************/

/**************************************************************************************************/
/*  devSequenceEvent File Description                                                             */
/**************************************************************************************************/

//==================================================================================================
//! @addtogroup Sequencer
//! @{
//!
//==================================================================================================
//! @file       devBasicSequence.cpp
//! @brief      EPICS Device Suppport for Basic Sequences
//!
//! @par Description:
//!   This file contains EPICS device support for event generator basic sequences.
//!   A sequence event can be represented in the EPICS data base by two required records and
//!   up to three more optional records. The records are distinguished by function codes specified
//!   in their I/O link fields (see below).  Basic Sequence records, may have the following
//!   function codes:
//!   - \b EVENT_CODE     (longout, required) Which event code to transmit.
//!   - \b EVENT_TIME     (ao, required) Specifies when in the sequence the event
//!                       should be transmitted.
//!   - \b EVENT_TIME     (ai, optional) Displays the actual timestamp assigned to the event.
//!                       This could differ from the requested timestamp because of event clock
//!                       quantization or because another event was assigned to that time.
//!   - \b EVENT_ENABLE   (bo, optional) Enables or disables event transmission.
//!   - \b EVENT_PRIORITY (longout, optional) When two sequence events end up with the same
//!                       timestamp, the relative priorities will determine which one gets
//!                       "jostled".
//!   @par
//!   Every event in a sequence must have a unique name associated with it -- even if the
//!   event code is duplicated.  The unique name is assigned when the SequenceEvent object is
//!   created.
//!
//! @par Device Type Field:
//!   A basic sequence record will have the "Device Type" field (DTYP) set to:<br>
//!      "EVG Basic Seq"
//!
//! @par Link Format:
//!   Basic sequence records use the INST_IO I/O link format. The INP or OUT links have the
//!   following format:<br>
//!      \@C=n; Seq=m; Fn=\<function\>
//!
//!   Where:
//!   - \b C     = Logical card number for the event generator card this sequence belongs to.
//!   - \b Seq   = Specifies the ID number of the sequence
//!   - \b Fn    = The record's function name (see list above)
//!
//==================================================================================================


/**************************************************************************************************/
/*  Imported Header Files                                                                         */
/**************************************************************************************************/

#include  <stdexcept>           // Standard C++ exception definitions
#include  <string>              // Standard C++ string class

#include  <epicsTypes.h>        // EPICS Architecture-independent type definitions

#include  <mrfCommon.h>         // MRF Common definitions
#include  <mrfIoLink.h>         // MRF I/O link field parser
#include  <drvSequence.h>       // MRF Sequence driver support declarations
#include  <evg/Sequence.h>      // MRF Sequence base class
#include  <BasicSequence.h>     // MRF Basic Sequence class
#include  <BasicSequenceEvent.h>// MRF Basic Sequence Event class

#include  <epicsExport.h>       // EPICS Symbol exporting macro definitions

/**************************************************************************************************/
/*  Structure Definitions                                                                         */
/**************************************************************************************************/

//=====================
// Common I/O link parameter definitions used by all BasicSequence records
//
static const
mrfParmNameList BasicSeqParmNames = {
    "C",        // Logical card number
    "Seq",      // Sequence number
    "Fn"        // Record function code
};

static const
epicsInt32  BasicSeqNumParms mrfParmNameListSize(BasicSeqParmNames);

//=====================
// BasicSequence Class ID String
//
const std::string BasicSequenceClassID(BASIC_SEQ_CLASS_ID);

/**************************************************************************************************/
/*                                Global Utility Routines                                         */
/*                                                                                                */


//**************************************************************************************************
//  EgDeclareBasicSequence () -- Declare The Existence Of A BasicSequence
//**************************************************************************************************
//! @par Description:
//!   Declare the existence of a basic sequence and return a pointer to its
//!   BasicSequence object.
//!
//! @par Function:
//!   First check to see if the sequence number already exists for the specified event generator
//!   card.  If so, make sure it is a BasicSequence object and return its pointer.
//!   If the sequence was not found, create a new  BasicSequence object, add it to the
//!   sequence list for this card, and return a pointer to its object.
//!
//! @param      Card    = (input) Logical card number of the event generator that this sequence
//!                       will belong to.
//! @param      SeqNum  = (input) Sequence number ID for this sequence.
//!
//! @return     Returns a pointer to the BasicSequence object.
//!
//! @throw      runtime_error is thrown if the sequence could not be created.
//!
//**************************************************************************************************

BasicSequence*
EgDeclareBasicSequence (epicsInt32 Card, epicsInt32 SeqNum) {

    //=====================
    // Local Variables
    //
    Sequence*       pSeq;       // Pointer to generic Sequence object
    BasicSequence*  pNewSeq;    // Pointer to new BasicSequence object

    //=====================
    // See if we already have a sequence for this card and sequence number
    //
    if (NULL != (pSeq = EgGetSequence (Card, SeqNum))) {

        //=====================
        // We found a sequence.  Make sure it is of the correct type
        //
        if (BasicSequenceClassID != pSeq->GetClassID()) {
            throw std::runtime_error (
                  std::string(pSeq->GetSeqID()) +
                  " is a " + pSeq->GetClassID() +
                  " object.  Expected a " + BasicSequenceClassID);
        }//end if this was the wrong type of sequence

        //=====================
        // A basic sequence already exists for this card and sequence number.
        // Return the pointer to it.
        //
        return (static_cast<BasicSequence*>(pSeq));

    }//end if sequence exists.

    //=====================
    // Sequence does not exist.
    // Create a new basic sequence for this card
    //
    pNewSeq = new BasicSequence (Card, SeqNum);

    //=====================
    // Try to add the sequence to the list of known sequences for this card
    //
    try {EgAddSequence(pNewSeq);}
    catch (std::exception& e) {
        delete pNewSeq;
        throw std::runtime_error(e.what());
    }//end if could not add the sequence to the list

    //=====================
    // If there were no errors, return the pointer to the new BasicSequence object
    //
    return (pNewSeq);

}//end EgDeclareBasicSequence()
