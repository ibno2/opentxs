/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#include <opentxs/core/stdafx.hpp>

#include <opentxs/ext/OTPayment.hpp>
#include <opentxs/ext/InstantiateContract.hpp>

#include <opentxs/cash/Purse.hpp>

#include <opentxs/core/recurring/OTPaymentPlan.hpp>
#include <opentxs/core/script/OTSmartContract.hpp>
#include <opentxs/core/Cheque.hpp>
#include <opentxs/core/util/Tag.hpp>
#include <opentxs/core/Log.hpp>

#include <irrxml/irrXML.hpp>

#include <memory>

namespace opentxs
{

char const* const __TypeStringsPayment[] = {

    // OTCheque is derived from OTTrackable, which is derived from OTInstrument,
    // which is
    // derived from OTScriptable, which is derived from OTContract.
    "CHEQUE",  // A cheque drawn on a user's account.
    "VOUCHER", // A cheque drawn on a server account (cashier's cheque aka
               // banker's cheque)
    "INVOICE", // A cheque with a negative amount. (Depositing this causes a
               // payment out, instead of a deposit in.)
    "PAYMENT PLAN",  // An OTCronItem-derived OTPaymentPlan, related to a
                     // recurring payment plan.
    "SMARTCONTRACT", // An OTCronItem-derived OTSmartContract, related to a
                     // smart contract.
    "PURSE",         // An OTContract-derived OTPurse containing a list of cash
                     // OTTokens.
    "ERROR_STATE"};

// static
const char* OTPayment::_GetTypeString(paymentType theType)
{
    int32_t nType = static_cast<int32_t>(theType);
    return __TypeStringsPayment[nType];
}

OTPayment::paymentType OTPayment::GetTypeFromString(const String& strType)
{
#define OT_NUM_ELEM(blah) (sizeof(blah) / sizeof(*(blah)))
    for (uint32_t i = 0; i < (OT_NUM_ELEM(__TypeStringsPayment) - 1); i++) {
        if (strType.Compare(__TypeStringsPayment[i]))
            return static_cast<OTPayment::paymentType>(i);
    }
#undef OT_NUM_ELEM
    return OTPayment::ERROR_STATE;
}

// Since the temp values are not available until at least ONE instantiating has
// occured,
// this function forces that very scenario (cleanly) so you don't have to
// instantiate-and-
// then-delete a payment instrument. Instead, just call this, and then the temp
// values will
// be available thereafter.
//
bool OTPayment::SetTempValues() // this version for OTTrackable (all types
                                // EXCEPT purses.)
{
    if (OTPayment::PURSE == m_Type) {
        // Perform instantiation of a purse, then use it to set the temp values,
        // then cleans it up again before returning success/fail.
        //
        Purse* pPurse = InstantiatePurse();

        if (nullptr == pPurse) {
            otErr << "OTPayment::SetTempValues: Error: Failed instantiating "
                     "OTPayment (purported purse) contents:\n\n" << m_strPayment
                  << "\n\n";
            return false;
        }
        std::unique_ptr<Purse> thePurseAngel(pPurse);

        return SetTempValuesFromPurse(*pPurse);
    }
    else {
        OTTrackable* pTrackable = Instantiate();

        if (nullptr == pTrackable) {
            otErr << "OTPayment::SetTempValues: Error: Failed instantiating "
                     "OTPayment contents:\n\n" << m_strPayment << "\n\n";
            return false;
        }
        // BELOW THIS POINT, MUST DELETE pTrackable!
        std::unique_ptr<OTTrackable> theTrackableAngel(pTrackable);

        Cheque* pCheque = nullptr;
        OTPaymentPlan* pPaymentPlan = nullptr;
        OTSmartContract* pSmartContract = nullptr;

        switch (m_Type) {
        case CHEQUE:
        case VOUCHER:
        case INVOICE:
            pCheque = dynamic_cast<Cheque*>(pTrackable);
            if (nullptr == pCheque)
                otErr << "OTPayment::SetTempValues: Failure: "
                         "dynamic_cast<OTCheque *>(pTrackable). Contents:\n\n"
                      << m_strPayment << "\n\n";
            // Let's grab all the temp values from the cheque!!
            //
            else // success
                return SetTempValuesFromCheque(*pCheque);
            break;

        case PAYMENT_PLAN:
            pPaymentPlan = dynamic_cast<OTPaymentPlan*>(pTrackable);
            if (nullptr == pPaymentPlan)
                otErr << "OTPayment::SetTempValues: Failure: "
                         "dynamic_cast<OTPaymentPlan *>(pTrackable). "
                         "Contents:\n\n" << m_strPayment << "\n\n";
            // Let's grab all the temp values from the payment plan!!
            //
            else // success
                return SetTempValuesFromPaymentPlan(*pPaymentPlan);
            break;

        case SMART_CONTRACT:
            pSmartContract = dynamic_cast<OTSmartContract*>(pTrackable);
            if (nullptr == pSmartContract)
                otErr << "OTPayment::SetTempValues: Failure: "
                         "dynamic_cast<OTSmartContract *>(pTrackable). "
                         "Contents:\n\n" << m_strPayment << "\n\n";
            // Let's grab all the temp values from the smart contract!!
            //
            else // success
                return SetTempValuesFromSmartContract(*pSmartContract);
            break;

        default:
            otErr << "OTPayment::SetTempValues: Failure: Wrong m_Type. "
                     "Contents:\n\n" << m_strPayment << "\n\n";
            return false;
        }
    }

    return false; // Should never actually reach this point.
}

bool OTPayment::SetTempValuesFromCheque(const Cheque& theInput)
{
    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:

        m_bAreTempValuesSet = true;

        m_lAmount = theInput.GetAmount();
        m_lTransactionNum = theInput.GetTransactionNum();

        if (theInput.GetMemo().Exists())
            m_strMemo.Set(theInput.GetMemo());
        else
            m_strMemo.Release();

        m_InstrumentDefinitionID = theInput.GetInstrumentDefinitionID();
        m_NotaryID = theInput.GetNotaryID();

        m_SenderNymID = theInput.GetSenderNymID();
        m_SenderAcctID = theInput.GetSenderAcctID();

        if (theInput.HasRecipient()) {
            m_bHasRecipient = true;
            m_RecipientNymID = theInput.GetRecipientNymID();
        }
        else {
            m_bHasRecipient = false;
            m_RecipientNymID.Release();
        }

        if (theInput.HasRemitter()) {
            m_bHasRemitter = true;
            m_RemitterNymID = theInput.GetRemitterNymID();
            m_RemitterAcctID = theInput.GetRemitterAcctID();
        }
        else {
            m_bHasRemitter = false;
            m_RemitterNymID.Release();
            m_RemitterAcctID.Release();
        }

        // NOTE: the "Recipient Acct" is NOT KNOWN when cheque is written, but
        // only
        // once the cheque gets deposited. Therefore if type is CHEQUE, then
        // Recipient
        // Acct ID is not set, and attempts to read it will result in failure.
        //
        m_RecipientAcctID.Release();

        m_VALID_FROM = theInput.GetValidFrom();
        m_VALID_TO = theInput.GetValidTo();

        return true;

    default:
        otErr << "OTPayment::SetTempValuesFromCheque: Error: Wrong type. "
                 "(Returning false.)\n";
        break;
    }

    return false;
}

bool OTPayment::SetTempValuesFromPaymentPlan(const OTPaymentPlan& theInput)
{
    if (OTPayment::PAYMENT_PLAN == m_Type) {
        m_bAreTempValuesSet = true;
        m_bHasRecipient = true;
        m_bHasRemitter = false;

        m_lAmount = theInput.GetInitialPaymentAmount(); // There're also regular
                                                        // payments of
        // GetPaymentPlanAmount(). Can't
        // fit 'em all.
        m_lTransactionNum = theInput.GetTransactionNum();

        // const OTString&  OTPaymentPlan::GetConsideration() const
        //                  { return m_strConsideration; }
        if (theInput.GetConsideration().Exists())
            m_strMemo.Set(theInput.GetConsideration());
        else
            m_strMemo.Release();

        m_InstrumentDefinitionID = theInput.GetInstrumentDefinitionID();
        m_NotaryID = theInput.GetNotaryID();

        m_SenderNymID = theInput.GetSenderNymID();
        m_SenderAcctID = theInput.GetSenderAcctID();

        m_RecipientNymID = theInput.GetRecipientNymID();
        m_RecipientAcctID = theInput.GetRecipientAcctID();

        m_RemitterNymID.Release();
        m_RemitterAcctID.Release();

        m_VALID_FROM = theInput.GetValidFrom();
        m_VALID_TO = theInput.GetValidTo();

        return true;
    }
    else
        otErr << "OTPayment::SetTempValuesFromPaymentPlan: Error: Wrong type. "
                 "(Returning false.)\n";

    return false;
}

bool OTPayment::SetTempValuesFromSmartContract(const OTSmartContract& theInput)
{
    if (OTPayment::SMART_CONTRACT == m_Type) {
        m_bAreTempValuesSet = true;
        m_bHasRecipient = false;
        m_bHasRemitter = false;

        m_lAmount = 0; // not used here.
        m_lTransactionNum = theInput.GetTransactionNum();

        // Note: Maybe later, store the Smart Contract's temporary name, or ID,
        // in the memo field.
        // Or something.
        //
        m_strMemo.Release(); // not used here.

        m_NotaryID = theInput.GetNotaryID();
        m_InstrumentDefinitionID.Release(); // not used here.

        m_SenderNymID = theInput.GetSenderNymID();
        m_SenderAcctID.Release();

        m_RecipientNymID.Release();  // not used here.
        m_RecipientAcctID.Release(); // not used here.

        m_RemitterNymID.Release();
        m_RemitterAcctID.Release();

        m_VALID_FROM = theInput.GetValidFrom();
        m_VALID_TO = theInput.GetValidTo();

        return true;
    }
    else
        otErr << __FUNCTION__ << ": Error: Wrong type. (Returning false.)\n";

    return false;
}

bool OTPayment::SetTempValuesFromPurse(const Purse& theInput)
{
    if (OTPayment::PURSE == m_Type) {
        m_bAreTempValuesSet = true;
        m_bHasRecipient = theInput.IsNymIDIncluded();
        m_bHasRemitter = false;

        m_lAmount = theInput.GetTotalValue();
        m_lTransactionNum = 0; // (A purse has no transaction number.)

        m_strMemo.Release(); // So far there's no purse memo (could add it,
                             // though.)

        m_InstrumentDefinitionID = theInput.GetInstrumentDefinitionID();
        m_NotaryID = theInput.GetNotaryID();

        m_SenderNymID.Release();
        m_SenderAcctID.Release();

        if (!m_bHasRecipient || !theInput.GetNymID(m_RecipientNymID)) {
            m_bHasRecipient = false;
            m_RecipientNymID.Release();
        }

        m_RecipientAcctID.Release();

        m_RemitterNymID.Release();
        m_RemitterAcctID.Release();

        m_VALID_FROM = theInput.GetLatestValidFrom();
        m_VALID_TO = theInput.GetEarliestValidTo();

        return true;
    }
    else
        otErr << "OTPayment::SetTempValuesFromPurse: Error: Wrong type. "
                 "(Returning false.)\n";

    return false;
}

bool OTPayment::GetMemo(String& strOutput) const
{
    strOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
        if (m_strMemo.Exists()) {
            strOutput = m_strMemo;
            bSuccess = true;
        }
        else
            bSuccess = false;
        break;

    case OTPayment::SMART_CONTRACT:
    case OTPayment::PURSE:
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetMemo: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetAmount(int64_t& lOutput) const
{
    lOutput = 0;

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::PURSE:
        lOutput = m_lAmount;
        bSuccess = true;
        break;

    case OTPayment::SMART_CONTRACT:
        lOutput = 0;
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetAmount: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetAllTransactionNumbers(NumList& numlistOutput) const
{
    // SMART CONTRACTS and PAYMENT PLANS get a little special
    // treatment here at the top.
    //
    if ((false == m_bAreTempValuesSet) || // Why is this here? Because if temp
                                          // values haven't been set yet,
        (OTPayment::SMART_CONTRACT ==
         m_Type) || // then m_Type isn't set either. We only want smartcontracts
                    // and   UPDATE: m_Type IS set!!
        (OTPayment::PAYMENT_PLAN == m_Type)) // payment plans here, but without
                                             // m_Type we can't know the
                                             // type...This comment is wrong!!
    {
        OTTrackable* pTrackable = Instantiate();
        if (nullptr == pTrackable) {
            otErr << __FUNCTION__
                  << ": Failed instantiating OTPayment containing:\n"
                  << m_strPayment << "\n";
            return false;
        } // Below THIS POINT, MUST DELETE pTrackable!
        std::unique_ptr<OTTrackable> theTrackableAngel(pTrackable);

        OTPaymentPlan* pPlan = nullptr;
        OTSmartContract* pSmartContract = nullptr;

        pPlan = dynamic_cast<OTPaymentPlan*>(pTrackable);
        pSmartContract = dynamic_cast<OTSmartContract*>(pTrackable);

        if (nullptr != pPlan) {
            pPlan->GetAllTransactionNumbers(numlistOutput);
            return true;
        }
        else if (nullptr != pSmartContract) {
            pSmartContract->GetAllTransactionNumbers(numlistOutput);
            return true;
        }
    }

    if (!m_bAreTempValuesSet) // (This function normally fails, if temp values
                              // aren't set,
        return false; //  for all payment types except the recurring ones
                      // above.)

    // Next: ALL OTHER payment types...
    //
    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
        if (m_lTransactionNum > 0) numlistOutput.Add(m_lTransactionNum);
        bSuccess = true;
        break;

    case OTPayment::PURSE:
        bSuccess = false;
        break;

    default:
    case OTPayment::PAYMENT_PLAN:   // Should never happen. (Handled already
                                    // above.)
    case OTPayment::SMART_CONTRACT: // Should never happen. (Handled already
                                    // above.)
        otErr << "OTPayment::" << __FUNCTION__ << ": Bad payment type!\n";
        break;
    }

    return bSuccess;
}

// This works with a cheque who has a transaction number.
// It also works with a payment plan or smart contract, for opening AND closing
// numbers.
bool OTPayment::HasTransactionNum(const int64_t& lInput) const
{
    // SMART CONTRACTS and PAYMENT PLANS get a little special
    // treatment here at the top.
    //
    if ((false == m_bAreTempValuesSet) || // Why is this here? Because if temp
                                          // values haven't been set yet,
        (OTPayment::SMART_CONTRACT ==
         m_Type) || // then m_Type isn't set either. We only want smartcontracts
                    // and   UPDATE: m_Type IS set!!
        (OTPayment::PAYMENT_PLAN == m_Type)) // payment plans here, but without
                                             // m_Type we can't know the
                                             // type...This comment is wrong!!
    {
        OTTrackable* pTrackable = Instantiate();
        if (nullptr == pTrackable) {
            otErr << __FUNCTION__
                  << ": Failed instantiating OTPayment containing:\n"
                  << m_strPayment << "\n";
            return false;
        } // BELOW THIS POINT, MUST DELETE pTrackable!
        std::unique_ptr<OTTrackable> theTrackableAngel(pTrackable);

        OTPaymentPlan* pPlan = nullptr;
        OTSmartContract* pSmartContract = nullptr;

        pPlan = dynamic_cast<OTPaymentPlan*>(pTrackable);
        pSmartContract = dynamic_cast<OTSmartContract*>(pTrackable);

        if (nullptr != pPlan) {
            if (pPlan->HasTransactionNum(lInput)) return true;
            return false;
        }
        else if (nullptr != pSmartContract) {
            if (pSmartContract->HasTransactionNum(lInput)) return true;
            return false;
        }
    }

    if (!m_bAreTempValuesSet) // (This function normally fails, if temp values
                              // aren't set,
        return false; //  for all payment types except the recurring ones
                      // above.)

    // Next: ALL OTHER payment types...
    //
    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
        if (lInput == m_lTransactionNum) bSuccess = true;
        break;

    case OTPayment::PURSE:
        bSuccess = false;
        break;

    default:
    case OTPayment::PAYMENT_PLAN:   // Should never happen. (Handled already
                                    // above.)
    case OTPayment::SMART_CONTRACT: // Should never happen. (Handled already
                                    // above.)
        otErr << "OTPayment::" << __FUNCTION__ << ": Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetClosingNum(int64_t& lOutput,
                              const Identifier& theAcctID) const
{
    lOutput = 0;

    // SMART CONTRACTS and PAYMENT PLANS get a little special
    // treatment here at the top.
    //
    if ((false ==
         m_bAreTempValuesSet) || // m_Type isn't even set if this is false.
        (OTPayment::SMART_CONTRACT == m_Type) ||
        (OTPayment::PAYMENT_PLAN == m_Type)) {
        OTTrackable* pTrackable = Instantiate();
        if (nullptr == pTrackable) {
            otErr << __FUNCTION__
                  << ": Failed instantiating OTPayment containing:\n"
                  << m_strPayment << "\n";
            return false;
        } // BELOW THIS POINT, MUST DELETE pTrackable!
        std::unique_ptr<OTTrackable> theTrackableAngel(pTrackable);

        OTSmartContract* pSmartContract = nullptr;
        pSmartContract = dynamic_cast<OTSmartContract*>(pTrackable);

        OTPaymentPlan* pPlan = nullptr;
        pPlan = dynamic_cast<OTPaymentPlan*>(pTrackable);

        if (nullptr != pSmartContract) {
            lOutput = pSmartContract->GetClosingNumber(theAcctID);
            if (lOutput > 0) return true;
            return false;
        }
        else if (nullptr != pPlan) {
            lOutput = pPlan->GetClosingNumber(theAcctID);
            if (lOutput > 0) return true;
            return false;
        }
    }

    if (!m_bAreTempValuesSet) return false;

    // Next: ALL OTHER payment types...
    //
    bool bSuccess = false;

    switch (m_Type) {

    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PURSE:
        lOutput = 0; // Redundant, but just making sure.
        bSuccess = false;
        break;

    default:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::SMART_CONTRACT:
        otErr << __FUNCTION__ << ": Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetOpeningNum(int64_t& lOutput,
                              const Identifier& theNymID) const
{
    lOutput = 0;

    // SMART CONTRACTS and PAYMENT PLANS get a little special
    // treatment here at the top.
    //
    if ((false ==
         m_bAreTempValuesSet) || // m_Type isn't even set if this is false.
        (OTPayment::SMART_CONTRACT == m_Type) ||
        (OTPayment::PAYMENT_PLAN == m_Type)) {
        OTTrackable* pTrackable = Instantiate();
        if (nullptr == pTrackable) {
            otErr << __FUNCTION__
                  << ": Failed instantiating OTPayment containing:\n"
                  << m_strPayment << "\n";
            return false;
        } // BELOW THIS POINT, MUST DELETE pTrackable!
        std::unique_ptr<OTTrackable> theTrackableAngel(pTrackable);

        OTSmartContract* pSmartContract = nullptr;
        pSmartContract = dynamic_cast<OTSmartContract*>(pTrackable);

        OTPaymentPlan* pPlan = nullptr;
        pPlan = dynamic_cast<OTPaymentPlan*>(pTrackable);

        if (nullptr != pSmartContract) {
            lOutput = pSmartContract->GetOpeningNumber(theNymID);
            if (lOutput > 0) return true;
            return false;
        }
        else if (nullptr != pPlan) {
            lOutput = pPlan->GetOpeningNumber(theNymID);
            if (lOutput > 0) return true;
            return false;
        }
    }

    if (!m_bAreTempValuesSet) return false;

    //
    // Next: ALL OTHER payment types...
    //
    bool bSuccess = false;

    switch (m_Type) {

    case OTPayment::CHEQUE:
    case OTPayment::INVOICE:
        if (theNymID == m_SenderNymID) {
            lOutput = m_lTransactionNum; // The "opening" number for a cheque is
                                         // the ONLY number it has.
            bSuccess = true;
        }
        else {
            lOutput = 0;
            bSuccess = false;
        }
        break;

    case OTPayment::VOUCHER:
        if (theNymID == m_RemitterNymID) {
            lOutput = m_lTransactionNum; // The "opening" number for a cheque is
                                         // the ONLY number it has.
            bSuccess = true;
        }
        else {
            lOutput = 0;
            bSuccess = false;
        }
        break;

    case OTPayment::PURSE:
        lOutput = 0; // Redundant, but just making sure.
        bSuccess = false;
        break;

    default:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::SMART_CONTRACT:
        otErr << __FUNCTION__ << ": Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetTransactionNum(int64_t& lOutput) const
{
    lOutput = 0;

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:   // For payment plans, this is the opening
                                    // transaction FOR THE NYM who activated the
                                    // contract (probably the customer.)
    case OTPayment::SMART_CONTRACT: // For smart contracts, this is the opening
                                    // transaction number FOR THE NYM who
                                    // activated the contract.
        lOutput = m_lTransactionNum;
        bSuccess = true;
        break;

    case OTPayment::PURSE:
        lOutput = 0;
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetTransactionNum: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetValidFrom(time64_t& tOutput) const
{
    tOutput = OT_TIME_ZERO;

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::PURSE:
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::SMART_CONTRACT:
        tOutput = m_VALID_FROM;
        bSuccess = true;
        break;

    default:
        otErr << "OTPayment::GetValidFrom: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetValidTo(time64_t& tOutput) const
{
    tOutput = OT_TIME_ZERO;

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::PURSE:
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::SMART_CONTRACT:
        tOutput = m_VALID_TO;
        bSuccess = true;
        break;

    default:
        otErr << "OTPayment::GetValidTo: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

// Verify whether the CURRENT date is AFTER the the VALID TO date.
// Notice, this will return false, if the instrument is NOT YET VALID.
// You have to use VerifyCurrentDate() to make sure you're within the
// valid date range to use this instrument. But sometimes you only want
// to know if it's expired, regardless of whether it's valid yet. So this
// function answers that for you.
//
bool OTPayment::IsExpired(bool& bExpired)
{
    if (!m_bAreTempValuesSet) return false;

    const time64_t CURRENT_TIME = OTTimeGetCurrentTime();

    // If the current time is AFTER the valid-TO date,
    // AND the valid_to is a nonzero number (0 means "doesn't expire")
    // THEN return true (it's expired.)
    //
    if ((CURRENT_TIME >= m_VALID_TO) && (m_VALID_TO > OT_TIME_ZERO))
        bExpired = true;
    else
        bExpired = false;

    return true;
}

// Verify whether the CURRENT date is WITHIN the VALID FROM / TO dates.
//
bool OTPayment::VerifyCurrentDate(bool& bVerified)
{
    if (!m_bAreTempValuesSet) return false;

    const time64_t CURRENT_TIME = OTTimeGetCurrentTime();

    if ((CURRENT_TIME >= m_VALID_FROM) &&
        ((CURRENT_TIME <= m_VALID_TO) || (OT_TIME_ZERO == m_VALID_TO)))
        bVerified = true;
    else
        bVerified = false;

    return true;
}

bool OTPayment::GetInstrumentDefinitionID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::PURSE:
        theOutput = m_InstrumentDefinitionID;
        bSuccess = true;
        break;

    case OTPayment::SMART_CONTRACT:
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetInstrumentDefinitionID: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetNotaryID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::SMART_CONTRACT:
    case OTPayment::PURSE:
        theOutput = m_NotaryID;
        bSuccess = true;
        break;

    default:
        otErr << "OTPayment::GetNotaryID: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

// With a voucher (cashier's cheque) the "bank" is the "sender",
// whereas the actual Nym who purchased it is the "remitter."
//
bool OTPayment::GetRemitterNymID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::VOUCHER:
        theOutput = m_RemitterNymID;
        bSuccess = true;
        break;

    default:
        otErr << "OTPayment::GetRemitterNymID: Bad payment type! Expected a "
                 "voucher cheque.\n";
        break;
    }

    return bSuccess;
}

// With a voucher (cashier's cheque) the "bank"s account is the "sender" acct,
// whereas the actual account originally used to purchase it is the "remitter"
// acct.
//
bool OTPayment::GetRemitterAcctID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::VOUCHER:
        theOutput = m_RemitterAcctID;
        bSuccess = true;
        break;

    default:
        otErr << "OTPayment::GetRemitterAcctID: Bad payment type! Expected a "
                 "voucher cheque.\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetSenderNymIDForDisplay(Identifier& theOutput) const
{
    if (IsVoucher()) return GetRemitterNymID(theOutput);

    return GetSenderNymID(theOutput);
}

bool OTPayment::GetSenderAcctIDForDisplay(Identifier& theOutput) const
{
    if (IsVoucher()) return GetRemitterAcctID(theOutput);

    return GetSenderAcctID(theOutput);
}

bool OTPayment::GetSenderNymID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::SMART_CONTRACT:
        theOutput = m_SenderNymID;
        bSuccess = true;
        break;

    case OTPayment::PURSE:
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetSenderNymID: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetSenderAcctID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
        theOutput = m_SenderAcctID;
        bSuccess = true;
        break;

    case OTPayment::SMART_CONTRACT:
    case OTPayment::PURSE:
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetSenderAcctID: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetRecipientNymID(Identifier& theOutput) const
{
    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::PAYMENT_PLAN:
    case OTPayment::PURSE:
        if (m_bHasRecipient) {
            theOutput = m_RecipientNymID;
            bSuccess = true;
        }
        else
            bSuccess = false;

        break;

    case OTPayment::SMART_CONTRACT:
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetRecipientNymID: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

bool OTPayment::GetRecipientAcctID(Identifier& theOutput) const
{
    // NOTE:
    // A cheque HAS NO "Recipient Asset Acct ID", since the recipient's account
    // (where he deposits
    // the cheque) is not known UNTIL the time of the deposit. It's certain not
    // known at the time
    // that the cheque is written...

    theOutput.Release();

    if (!m_bAreTempValuesSet) return false;

    bool bSuccess = false;

    switch (m_Type) {
    case OTPayment::PAYMENT_PLAN:
        if (m_bHasRecipient) {
            theOutput = m_RecipientAcctID;
            bSuccess = true;
        }
        else
            bSuccess = false;

        break;

    case OTPayment::CHEQUE:
    case OTPayment::VOUCHER:
    case OTPayment::INVOICE:
    case OTPayment::SMART_CONTRACT:
    case OTPayment::PURSE: // A purse might have a recipient USER, but never a
                           // recipient ACCOUNT.
        bSuccess = false;
        break;

    default:
        otErr << "OTPayment::GetRecipientAcctID: Bad payment type!\n";
        break;
    }

    return bSuccess;
}

OTPayment::OTPayment()
    : Contract()
    , m_Type(OTPayment::ERROR_STATE)
    , m_bAreTempValuesSet(false)
    , m_bHasRecipient(false)
    , m_bHasRemitter(false)
    , m_lAmount(0)
    , m_lTransactionNum(0)
    , m_VALID_FROM(OT_TIME_ZERO)
    , m_VALID_TO(OT_TIME_ZERO)
{
    InitPayment();
}

OTPayment::OTPayment(const String& strPayment)
    : Contract()
    , m_Type(OTPayment::ERROR_STATE)
    , m_bAreTempValuesSet(false)
    , m_bHasRecipient(false)
    , m_bHasRemitter(false)
    , m_lAmount(0)
    , m_lTransactionNum(0)
    , m_VALID_FROM(OT_TIME_ZERO)
    , m_VALID_TO(OT_TIME_ZERO)
{
    InitPayment();

    SetPayment(strPayment);
}

// CALLER is responsible to delete.
//
OTTrackable* OTPayment::Instantiate() const
{
    Contract* pContract = nullptr;
    OTTrackable* pTrackable = nullptr;
    Cheque* pCheque = nullptr;
    OTPaymentPlan* pPaymentPlan = nullptr;
    OTSmartContract* pSmartContract = nullptr;

    switch (m_Type) {
    case CHEQUE:
    case VOUCHER:
    case INVOICE:
        pContract = ::InstantiateContract(m_strPayment);

        if (nullptr != pContract) {
            pCheque = dynamic_cast<Cheque*>(pContract);

            if (nullptr == pCheque) {
                otErr << "OTPayment::Instantiate: Tried to instantiate cheque, "
                         "but factory returned non-cheque:\n\n" << m_strPayment
                      << "\n\n";
                delete pContract;
                pContract = nullptr;
            }
            else
                pTrackable = pCheque;
        }
        else
            otErr << "OTPayment::Instantiate: Tried to instantiate cheque, but "
                     "factory returned nullptr:\n\n" << m_strPayment << "\n\n";
        break;

    case PAYMENT_PLAN:
        pContract = ::InstantiateContract(m_strPayment);

        if (nullptr != pContract) {
            pPaymentPlan = dynamic_cast<OTPaymentPlan*>(pContract);

            if (nullptr == pPaymentPlan) {
                otErr << "OTPayment::Instantiate: Tried to instantiate payment "
                         "plan, but factory returned non-payment-plan:\n\n"
                      << m_strPayment << "\n\n";
                delete pContract;
                pContract = nullptr;
            }
            else
                pTrackable = pPaymentPlan;
        }
        else
            otErr << "OTPayment::Instantiate: Tried to instantiate payment "
                     "plan, but factory returned nullptr:\n\n" << m_strPayment
                  << "\n\n";
        break;

    case SMART_CONTRACT:
        pContract = ::InstantiateContract(m_strPayment);

        if (nullptr != pContract) {
            pSmartContract = dynamic_cast<OTSmartContract*>(pContract);

            if (nullptr == pSmartContract) {
                otErr << __FUNCTION__
                      << ": Tried to instantiate smart contract, but factory "
                         "returned non-smart-contract:\n\n" << m_strPayment
                      << "\n\n";
                delete pContract;
                pContract = nullptr;
            }
            else
                pTrackable = pSmartContract;
        }
        else
            otErr << "OTPayment::Instantiate: Tried to instantiate smart "
                     "contract, but factory returned nullptr:\n\n"
                  << m_strPayment << "\n\n";
        break;
    case PURSE:
        otErr << "OTPayment::Instantiate: ERROR: Tried to instantiate purse, "
                 "but should have called OTPayment::InstantiatePurse.\n";
        return nullptr;
    default:
        otErr << "OTPayment::Instantiate: ERROR: Tried to instantiate payment "
                 "object, but had a bad type. Contents:\n\n" << m_strPayment
              << "\n\n";
        return nullptr;
    }

    return pTrackable;
}

OTTrackable* OTPayment::Instantiate(const String& strPayment)
{
    if (SetPayment(strPayment)) return Instantiate();

    return nullptr;
}

// You need the server ID to instantiate a purse, unlike all the
// other payment types. UPDATE: Not anymore.
//
// CALLER is responsible to delete!
//
Purse* OTPayment::InstantiatePurse() const
{
    if (OTPayment::PURSE == GetType()) {
        return Purse::PurseFactory(m_strPayment);
    }
    else
        otErr << "OTPayment::InstantiatePurse: Failure: This payment object "
                 "does NOT contain a purse. "
                 "Contents:\n\n" << m_strPayment << "\n\n";

    return nullptr;
}

/*
OTPurse * OTPayment::InstantiatePurse(const OTIdentifier& NOTARY_ID) const
{
    if (OTPayment::PURSE == GetType())
    {
        return OTPurse::PurseFactory(m_strPayment, NOTARY_ID);
       }
    else
        otErr << "OTPayment::InstantiatePurse: Failure: This payment object does
NOT contain a purse. "
                      "Contents:\n\n%s\n\n", m_strPayment.Get());

    return nullptr;
}

OTPurse * OTPayment::InstantiatePurse(const OTIdentifier& NOTARY_ID, const
OTIdentifier& INSTRUMENT_DEFINITION_ID) const
{
    if (OTPayment::PURSE == GetType())
    {
        return OTPurse::PurseFactory(m_strPayment, NOTARY_ID,
INSTRUMENT_DEFINITION_ID);
       }
    else
        otErr << "OTPayment::InstantiatePurse: Failure: This payment object does
NOT contain a purse. "
                      "Contents:\n\n%s\n\n", m_strPayment.Get());

    return nullptr;
}
*/

Purse* OTPayment::InstantiatePurse(const String& strPayment)
{
    if (!SetPayment(strPayment))
        otErr << "OTPayment::InstantiatePurse: WARNING: Failed setting the "
                 "payment string based on "
                 "what was passed in:\n\n" << strPayment << "\n\n";
    else if (OTPayment::PURSE != m_Type)
        otErr << "OTPayment::InstantiatePurse: WARNING: No purse was found in "
                 "the "
                 "payment string:\n\n" << strPayment << "\n\n";
    else
        return InstantiatePurse();

    return nullptr;
}

/*
OTPurse * OTPayment::InstantiatePurse(const OTIdentifier& NOTARY_ID, const
OTString& strPayment)
{
    if (!SetPayment(strPayment))
        otErr << "OTPayment::InstantiatePurse: WARNING: Failed setting the
payment string based on "
                      "what was passed in:\n\n%s\n\n", strPayment.Get());
    else if (OTPayment::PURSE != m_Type)
        otErr << "OTPayment::InstantiatePurse: WARNING: No purse was found in
the "
                      "payment string:\n\n%s\n\n", strPayment.Get());
    else
        return InstantiatePurse(NOTARY_ID);

    return nullptr;
}


OTPurse * OTPayment::InstantiatePurse(const OTIdentifier& NOTARY_ID, const
OTIdentifier& INSTRUMENT_DEFINITION_ID, const OTString& strPayment)
{
    if (!SetPayment(strPayment))
        otErr << "OTPayment::InstantiatePurse: WARNING: Failed setting the
payment string based on "
                      "what was passed in:\n\n%s\n\n", strPayment.Get());
    else if (OTPayment::PURSE != m_Type)
        otErr << "OTPayment::InstantiatePurse: WARNING: No purse was found in
the "
                      "payment string:\n\n%s\n\n", strPayment.Get());
    else
        return InstantiatePurse(NOTARY_ID, INSTRUMENT_DEFINITION_ID);

    return nullptr;
}
*/

bool OTPayment::SetPayment(const String& strPayment)
{
    if (!strPayment.Exists()) return false;

    String strContract(strPayment);

    if (!strContract.DecodeIfArmored(false)) // bEscapedIsAllowed=true
                                             // by default.
    {
        otErr << __FUNCTION__ << ": Input string apparently was encoded and "
                                 "then failed decoding. Contents: \n"
              << strPayment << "\n";
        return false;
    }

    m_strPayment.Release();

    // todo: should be "starts with" and perhaps with a trim first
    //
    if (strContract.Contains("-----BEGIN SIGNED CHEQUE-----"))
        m_Type = OTPayment::CHEQUE;
    else if (strContract.Contains("-----BEGIN SIGNED VOUCHER-----"))
        m_Type = OTPayment::VOUCHER;
    else if (strContract.Contains("-----BEGIN SIGNED INVOICE-----"))
        m_Type = OTPayment::INVOICE;

    else if (strContract.Contains("-----BEGIN SIGNED PAYMENT PLAN-----"))
        m_Type = OTPayment::PAYMENT_PLAN;
    else if (strContract.Contains("-----BEGIN SIGNED SMARTCONTRACT-----"))
        m_Type = OTPayment::SMART_CONTRACT;

    else if (strContract.Contains("-----BEGIN SIGNED PURSE-----"))
        m_Type = OTPayment::PURSE;
    else {
        m_Type = OTPayment::ERROR_STATE;

        otErr << __FUNCTION__
              << ": Failure: Unable to determine payment type, from input:\n\n"
              << strContract << "\n\n";
    }

    if (OTPayment::ERROR_STATE == m_Type) return false;

    m_strPayment.Set(strContract);

    return true;
}

void OTPayment::InitPayment()
{
    m_Type = OTPayment::ERROR_STATE;
    m_lAmount = 0;
    m_lTransactionNum = 0;
    m_VALID_FROM = OT_TIME_ZERO;
    m_VALID_TO = OT_TIME_ZERO;
    m_bAreTempValuesSet = false;
    m_bHasRecipient = false;
    m_bHasRemitter = false;
    m_strContractType.Set("PAYMENT");
}

OTPayment::~OTPayment()
{
    Release_Payment();
}

void OTPayment::Release_Payment()
{
    m_Type = OTPayment::ERROR_STATE;
    m_lAmount = 0;
    m_lTransactionNum = 0;
    m_VALID_FROM = OT_TIME_ZERO;
    m_VALID_TO = OT_TIME_ZERO;

    m_strPayment.Release();

    m_bAreTempValuesSet = false;
    m_bHasRecipient = false;
    m_bHasRemitter = false;

    m_strMemo.Release();

    m_InstrumentDefinitionID.Release();
    m_NotaryID.Release();

    m_SenderNymID.Release();
    m_SenderAcctID.Release();
    m_RecipientNymID.Release();
    m_RecipientAcctID.Release();

    m_RemitterNymID.Release();
    m_RemitterAcctID.Release();
}

void OTPayment::Release()
{
    Release_Payment();

    // Finally, we call the method we overrode:
    //
    Contract::Release();
}

void OTPayment::UpdateContents()
{
    // I release this because I'm about to repopulate it.
    m_xmlUnsigned.Release();

    Tag tag("payment");

    tag.add_attribute("version", m_strVersion.Get());
    tag.add_attribute("type", GetTypeString());

    if (m_strPayment.Exists()) {
        const OTASCIIArmor ascContents(m_strPayment);

        if (ascContents.Exists()) {
            tag.add_tag("contents", ascContents.Get());
        }
    }

    std::string str_result;
    tag.output(str_result);

    m_xmlUnsigned.Concatenate("%s", str_result.c_str());
}

int32_t OTPayment::ProcessXMLNode(irr::io::IrrXMLReader*& xml)
{
    const String strNodeName(xml->getNodeName());

    if (strNodeName.Compare("payment")) {
        m_strVersion = xml->getAttributeValue("version");

        const String strPaymentType = xml->getAttributeValue("type");

        if (strPaymentType.Exists())
            m_Type = OTPayment::GetTypeFromString(strPaymentType);
        else
            m_Type = OTPayment::ERROR_STATE;

        otLog4 << "Loaded payment... Type: " << GetTypeString()
               << "\n----------\n";

        return (OTPayment::ERROR_STATE == m_Type) ? (-1) : 1;
    }
    else if (strNodeName.Compare("contents")) {
        String strContents;

        if (!Contract::LoadEncodedTextField(xml, strContents) ||
            !strContents.Exists() || !SetPayment(strContents)) {
            otErr << "OTPayment::ProcessXMLNode: ERROR: \"contents\" field "
                     "without a value, OR error setting that "
                     "value onto this object. Raw:\n\n" << strContents
                  << "\n\n";

            return (-1); // error condition
        }
        // else success -- the value is now set on this object.
        // todo security: Make sure the type of the payment that's ACTUALLY
        // there
        // matches the type expected (based on the m_Type that we already read,
        // above.)

        return 1;
    }

    return 0;
}

} // namespace opentxs
