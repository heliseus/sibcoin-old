
#include "dex.h"
#include "init.h"
#include "util.h"

#include "dexoffer.h"
#include "primitives/transaction.h"
#include "utilstrencodings.h"
#include "dex/db/dexdb.h"
#include "dextransaction.h"
#include "consensus/validation.h"
#include "chainparams.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif


#define CHECK(A,B,C) { if (!(A)) { std::string str = strprintf(std::string("%s") + (B) + ("; TransactionID=%s"), "",(C)); LogPrint("dex", "%s\n", str.c_str()); sError += str; break; } }

namespace dex {

CDex::CDex()
{
}


CDex::CDex(const CDexOffer &dexoffer)
{
    offer = dexoffer;
}


bool CDex::CreateOffer(CDexOffer::Type type, const uint256 &idTransaction, const std::string &pubKey, const std::string &countryIso, const std::string &currencyIso,
           uint8_t paymentMethod, uint64_t price, uint64_t minAmount, time_t timeExpiration,
           const std::string &shortInfo, const std::string &details, const uint32_t &editingVersion)
{
    return offer.Create(idTransaction, type, pubKey, countryIso, currencyIso, paymentMethod, price, minAmount, timeExpiration, shortInfo, details, editingVersion);
}


bool CDex::CreateOffer(CDexOffer::Type type, const std::string &pubKey, const std::string &countryIso, const std::string &currencyIso,
                        uint8_t paymentMethod, uint64_t price, uint64_t minAmount, time_t timeExpiration,
                        const std::string &shortInfo, const std::string &details, const uint32_t &editingVersion)
{
    return offer.Create(type, pubKey, countryIso, currencyIso, paymentMethod, price, minAmount, timeExpiration, shortInfo, details, editingVersion);
}

bool CDex::CreateOffer(const dex::MyOfferInfo &info)
{
    return offer.Create(info);
}



bool CDex::PayForOffer(uint256 &txid, std::string &sError)
{
    do {
        CHECK(!offer.IsNull(), "Offer is empty", offer.idTransaction.ToString());
        CHECK(offer.idTransaction.IsNull(), "The offer has already been paid", offer.idTransaction.ToString());

        CHECK(CreatePayOfferTransaction(offer, payTx, sError), "", offer.idTransaction.ToString());
        offer.idTransaction = payTx.GetHash();
        txid = offer.idTransaction;
        return true;
    } while(false);
    return false;
}


bool CDex::CheckOfferTx(std::string &sError)
{
    do {
        CHECK(!offer.IsNull(), "offer is NULL", offer.idTransaction.ToString());
        CHECK(offer.Check(true), "offer check fail", offer.idTransaction.ToString());

        CTransaction tx;
        uint256 hashBlock;
        CHECK (GetTransaction(offer.idTransaction, tx, Params().GetConsensus(), hashBlock, true), "Transaction not found", offer.idTransaction.ToString());

        if (!CheckTx(tx, sError)) break;

        CHECK(!hashBlock.IsNull(), "transaction in mempool, not in block", offer.idTransaction.ToString());
        int confirmations = -1;
        {
            BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                CBlockIndex* pindex = (*mi).second;
                if (chainActive.Contains(pindex)) {
                  confirmations = 1 + chainActive.Height() - pindex->nHeight;
                }
            }
        }
        CHECK(confirmations >= PAYOFFER_MIN_TX_HEIGHT, "not enough transaction confirmations", offer.idTransaction.ToString());

        return true;
    } while(false);
    return false;
}


bool CDex::CheckTx(const CTransaction &tx, std::string &sError)
{
    do {
        CHECK(tx.vin.size() > 0, "vin empty", offer.idTransaction.ToString());
        CHECK(tx.vout.size() > 0, "vout empty", offer.idTransaction.ToString());
        CHECK(tx.vout[0].nValue == PAYOFFER_RETURN_FEE, "bad op_return fee", offer.idTransaction.ToString());
        CHECK(tx.vout[0].scriptPubKey.IsUnspendable(), "not op_return", offer.idTransaction.ToString());

        {
            uint256 hash;
            opcodetype opcode;
            std::vector<unsigned char> vch;
            CScript::const_iterator pc = tx.vout[0].scriptPubKey.begin();
            while (pc < tx.vout[0].scriptPubKey.end()) {
                CHECK(tx.vout[0].scriptPubKey.GetOp(pc, opcode, vch), "fail to getop from script", offer.idTransaction.ToString());
                if (0 <= opcode && opcode <= OP_PUSHDATA4) hash.SetHex(HexStr(vch));
            }
            CHECK(offer.hash == hash, "offer hash not equal", offer.idTransaction.ToString());
        }

        CAmount credit = 0;
        for (size_t i = 0; i < tx.vout.size(); i++) {
            credit += tx.vout[i].nValue;
        }

        CAmount debit = 0;
        for (auto i : tx.vin) {
            CTransaction prevtx;
            uint256 hashBlock;
            CHECK (GetTransaction(i.prevout.hash, prevtx, Params().GetConsensus(), hashBlock, true), "vin tx not found", offer.idTransaction.ToString());
            CHECK (prevtx.vout.size() > i.prevout.n, "prev tx out error", offer.idTransaction.ToString());
            debit += prevtx.vout[i.prevout.n].nValue;
        }

        int days = ((offer.timeExpiration - offer.timeCreate - 1) / 86400) + 1;
        int coef = ((days - 1) / 10) + 1;
        CHECK((debit - credit) >= PAYOFFER_TX_FEE * coef, "payoffer tx fee error", offer.idTransaction.ToString());

        return true;
    } while(false);
    return false;
}



bool CDex::CheckBRCSTOfferTx(const CTransaction &tx, std::string &sError)
{
    do {
        CHECK(offer.idTransaction == tx.GetHash(), "transactions not equal", offer.idTransaction.ToString());

        // check transaction size
        CSizeComputer sc(SER_NETWORK, PROTOCOL_VERSION);
        sc << tx;
        CHECK(sc.size() <= MAX_TRANSACTION_SIZE, "transaction too large", tx.ToString());

        if (!CheckTx(tx, sError)) break;

        return true;
    } while (false);
    return false;
}



bool CDex::CheckOfferSign(const std::vector<unsigned char> &vchSign, std::string &sError)
{
    do {
        CHECK(!offer.IsNull(), "Offer is empty", offer.idTransaction.ToString());
        CHECK(!vchSign.empty(), "Offer sign is empty", offer.idTransaction.ToString());
        CPubKey pkey = offer.getPubKeyObject();
        CHECK(pkey.IsFullyValid(), "Invalid public key", offer.idTransaction.ToString());
        CHECK(pkey.Verify(offer.hash, vchSign), "Invalid offer sign", offer.idTransaction.ToString());
        return true;
    } while (false);
    return false;
}



bool CDex::FindKey(CKey &key, std::string &sError)
{
#ifndef ENABLE_WALLET
    sError = "wallet not enabled";
    return false;
#else
    do {
        CHECK(!offer.IsNull(), "Offer is empty", offer.idTransaction.ToString());
        CPubKey pkey = offer.getPubKeyObject();
        CHECK(pkey.IsFullyValid(), "Invalid public key", offer.idTransaction.ToString());
        CHECK(pwalletMain->GetKey(pkey.GetID(), key), "Private key not found in wallet", offer.idTransaction.ToString());
        return true;
    } while (false);
    return false;
#endif
}



bool CDex::SignOffer(const CKey &key, std::vector<unsigned char> &vchSign, std::string &sError)
{
    vchSign.clear();
#ifndef ENABLE_WALLET
    sError = "wallet not enabled";
    return false;
#else
    do {
        CHECK(!offer.IsNull(), "Offer is empty", offer.idTransaction.ToString());
        CHECK(key.Sign(offer.hash, vchSign), "Sign operation error", offer.idTransaction.ToString());
        return true;
    } while (false);
    return false;
#endif
}


bool CDex::CheckEditSign()
{
  return offer.CheckEditSign();
}



bool CDex::MakeEditSign(const CKey &key, std::string &sError)
{
#ifndef ENABLE_WALLET
    sError = "wallet not enabled";
    return false;
#else
    std::vector<unsigned char> vchSign;
    do {
        CHECK(!offer.IsNull(), "Offer is empty", offer.idTransaction.ToString());
        uint256 edithash = offer.MakeEditHash();
        CHECK(key.Sign(edithash, vchSign), "Sign operation error", offer.idTransaction.ToString());
        offer.editsign = HexStr(vchSign);
        return true;
    } while (false);
    return false;
#endif
}


const CTransaction & CDex::getPayTx() const
{
    return payTx;
}


}
