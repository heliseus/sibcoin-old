
#include "rpc/server.h"

#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "init.h"
#include "netbase.h"
#include "protocol.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"
#include "utilstrencodings.h"
#include "version.h"
#include <univalue.h>
#include "streams.h"
#include <algorithm>
#include "dexoffer.h"
#include "dexsync.h"
#include "random.h"
#include "dex/db/dexdb.h"
#include "dex.h"
#include "dex/db/dexdto.h"
#include "core_io.h"
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "dextransaction.h"
#include "parserjsonoffer.h"
#include "callbackdbforrpc.h"
#include "dexmanager.h"
#include "db/countryiso.h"
#include "db/currencyiso.h"
#include "db/defaultdatafordb.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif


using namespace std;


UniValue dexoffers(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp || params.size() < 1 || params.size() > 8)
        throw runtime_error(
            "dexoffers [buy|sell|all] [country] [currency] [payment_method] [limit N] [offset N]\n"
            "Get DEX offers list.\n"

            "\nArguments:\n"
            "NOTE: Any of the parameters may be skipped.You must specify at least one parameter.\n"
            "\tcountry         (string, optional) two-letter country code (ISO 3166-1 alpha-2 code).\n"
            "\tcurrency        (string, optional) three-letter currency code (ISO 4217).\n"
            "\tpayment_method  (string, optional, case insensitive) payment method name.\n"
            "\tlimit N         (int, optional) N max output offers, default use global settings"
            "\toffset N        (int, optional) N identify the starting point to return rows, use with limit"

            "\nResult (for example):\n"
            "[\n"
            "   {\n"
            "     \"type\"          : \"sell\",   offer type, buy or sell\n"
            "     \"idTransaction\" : \"<id>\",   transaction with offer fee\n"
            "     \"hash\"          : \"<hash>\", offer hash\n"
            "     \"countryIso\"    : \"RU\",     country (ISO 3166-1 alpha-2)\n"
            "     \"currencyIso\"   : \"RUB\",    currency (ISO 4217)\n"
            "     \"paymentMethod\" : 1,        payment method code (default 1 - cash, 128 - online)\n"
            "     \"price\"         : 10000,\n"
            "     \"minAmount\"     : 1000,\n"
            "     \"timeCreate\"    : 947...3344,\n"
            "     \"timeExpiration\": 947...9344, offer expiration (in seconds)\n"
            "     \"shortInfo\"     : \"...\",    offer short info (max 140 bytes)\n"
            "     \"details\"       : \"...\"     offer details (max 1024 bytes)\n"
            "   },\n"
            "   ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("dexoffers", "all USD")
            + HelpExampleCli("dexoffers", "RU RUB cash")
            + HelpExampleCli("dexoffers", "all USD online")
            + HelpExampleCli("dexoffers", "all USD limit 3")
            + HelpExampleCli("dexoffers", "all USD limit 3 offset 10")
        );

    UniValue result(UniValue::VARR);

    std::string typefilter, countryfilter, currencyfilter;
    std::string methodfilter;
    unsigned char methodfiltertype = 0;
    std::list<std::string> words {"buy", "sell", "all"};
    dex::CountryIso  countryiso;
    dex::CurrencyIso currencyiso;

    int limit = 0;
    int offset = 0;

    for (size_t i = 0; i < params.size(); i++) {
        if (params[i].get_str() == "limit") {
            if (i == 0 || params.size() <= i+1) {
                throw runtime_error("\nnot enough parameters\n");
            }

            std::string maxStr = params[i+1].get_str();
            limit = std::stoi(maxStr);

            if (params.size() > i+2) {
                i++;
                continue;
            } else {
                break;
            }
        }
        if (params[i].get_str() == "offset" && limit > 0) {
            if (i == 0 || params.size() <= i+1) {
                throw runtime_error("\nnot enough parameters\n");
            }

            std::string maxStr = params[i+1].get_str();
            offset = std::stoi(maxStr);
            break;
        }
        if (i == 0 && typefilter.empty()) {
            if (std::find(words.begin(), words.end(), params[0].get_str()) != words.end()) {
                typefilter = params[0].get_str();
                continue;
            } else {
                typefilter = "all";
            }
        }
        if (i < 2 && countryfilter.empty()) {
            if (countryiso.isValid(params[i].get_str())) {
                countryfilter = params[i].get_str();
                continue;
            }
        }
        if (i < 3 && currencyfilter.empty()) {
            if (currencyiso.isValid(params[i].get_str())) {
                currencyfilter = params[i].get_str();
                continue;
            }
        }
        {
            methodfilter.clear();
            std::string methodname = boost::algorithm::to_lower_copy(params[i].get_str());
            std::list<dex::PaymentMethodInfo> pms = dex::DexDB::self()->getPaymentMethodsInfo();
            for (auto j : pms) {
                std::string name = boost::algorithm::to_lower_copy(j.name);
                if (name == methodname) {
                    methodfilter = j.name;
                    methodfiltertype = j.type;
                }
            }

            if (methodfilter.empty()) {
                throw runtime_error("\nwrong parameter: " + params[i].get_str() + "\n");
            }
        }
    }

    if (typefilter.empty()) {
        throw runtime_error("\nwrong parameters\n");
    }

    // check country and currency in DB
    if (countryfilter != "") {
        dex::CountryInfo  countryinfo = dex::DexDB::self()->getCountryInfo(countryfilter);
        if (!countryinfo.enabled) {
            throw runtime_error("\nERROR: this country is disabled in DB\n");
        }
    }
    if (currencyfilter != "") {
        dex::CurrencyInfo  currencyinfo = dex::DexDB::self()->getCurrencyInfo(currencyfilter);
        if (!currencyinfo.enabled) {
            throw runtime_error("\nERROR: this currency is disabled in DB\n");
        }
    }

    if (limit == 0) {
        limit = maxOutput();
    }
    int step = 0;

    if (typefilter == "buy" || typefilter == "all") {
        std::list<dex::OfferInfo> offers = dex::DexDB::self()->getOffersBuy(countryfilter, currencyfilter, methodfiltertype, limit, offset);
        for (auto i : offers) {
            CDexOffer o(i, dex::Buy);
            result.push_back(o.getUniValue());

            if (limit > 0) {
                step++;

                if (step == limit) {
                    break;
                }
            }
        }
    }

    if ((typefilter == "sell" || typefilter == "all") && !(limit > 0 && step == limit)) {
        std::list<dex::OfferInfo> offers = dex::DexDB::self()->getOffersSell(countryfilter, currencyfilter, methodfiltertype, limit-step, offset);
        for (auto i : offers) {
            CDexOffer o(i, dex::Sell);
            result.push_back(o.getUniValue());
        }
    }

    return result;
}




UniValue dexmyoffers(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }
    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }
    if (fHelp || params.size() < 1 || params.size() > 9)
        throw runtime_error(
            "dexmyoffers [buy|sell|all] [country] [currency] [payment_method] [status] [limit N] [offset N]\n"
            "Return a list of  DEX own offers.\n"

            "\nArguments:\n"
            "NOTE: Any of the parameters may be skipped.You must specify at least one parameter.\n"
            "\tcountry         (string, optional) two-letter country code (ISO 3166-1 alpha-2 code).\n"
            "\tcurrency        (string, optional) three-letter currency code (ISO 4217).\n"
            "\tpayment_method  (string, optional, case insensitive) payment method name.\n"
            "\tstatus          (string, optional, case insensitive) offer status (Active,Draft,Expired,Cancelled,Suspended,Unconfirmed).\n"
            "\tlimit N         (int, optional) N max output offers, default use global settings"
            "\toffset N        (int, optional) N identify the starting point to return rows, use with limit"

            "\nResult (for example):\n"
            "[\n"
            "   {\n"
            "     \"type\"          : \"sell\",   offer type, buy or sell\n"
            "     \"status\"        : \"1\",      offer status\n"
            "     \"statusStr\"     : \"Draft\",  offer status name\n"
            "     \"idTransaction\" : \"<id>\",   transaction with offer fee\n"
            "     \"hash\"          : \"<hash>\", offer hash\n"
            "     \"pubKey\"        : \"<key>\",  offer public key\n"
            "     \"countryIso\"    : \"RU\",     country (ISO 3166-1 alpha-2)\n"
            "     \"currencyIso\"   : \"RUB\",    currency (ISO 4217)\n"
            "     \"paymentMethod\" : 1,        payment method code (default 1 - cash, 128 - online)\n"
            "     \"price\"         : 10000,\n"
            "     \"minAmount\"     : 1000,\n"
            "     \"timeCreate\"    : 947...9344,\n"
            "     \"timeExpiration\": 947...5344, offer expiration\n"
            "     \"shortInfo\"     : \"...\",    offer short info (max 140 bytes)\n"
            "     \"details\"       : \"...\"     offer details (max 1024 bytes)\n"
            "   },\n"
            "   ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("dexmyoffers", "all USD")
            + HelpExampleCli("dexmyoffers", "RU RUB cash")
            + HelpExampleCli("dexmyoffers", "all USD online")
            + HelpExampleCli("dexmyoffers", "all USD limit 3")
            + HelpExampleCli("dexmyoffers", "all USD limit 3 offset 10")
        );

    UniValue result(UniValue::VARR);

    std::string typefilter, countryfilter, currencyfilter, methodfilter;
    int statusfilter = dex::Indefined;
    dex::CStatusOffer status;
    unsigned char methodfiltertype = 0;
    std::map<std::string, int> words;
    words["buy"] = 0;
    words["sell"] = 1;
    words["all"] = -1;
    dex::CountryIso  countryiso;
    dex::CurrencyIso currencyiso;

    int limit = 0;
    int offset = 0;

    for (size_t i = 0; i < params.size(); i++) {
        if (params[i].get_str() == "limit") {
            if (i == 0 || params.size() <= i+1) {
                throw runtime_error("\nnot enough parameters\n");
            }

            std::string maxStr = params[i+1].get_str();
            limit = std::stoi(maxStr);

            if (params.size() > i+2) {
                i++;
                continue;
            } else {
                break;
            }
        } else if (params[i].get_str() == "offset" && limit > 0) {
            if (i == 0 || params.size() <= i+1) {
                throw runtime_error("\nnot enough parameters\n");
            }

            std::string maxStr = params[i+1].get_str();
            offset = std::stoi(maxStr);
            break;
        } else {
            if (typefilter.empty()) {
                std::string key = boost::algorithm::to_lower_copy(params[i].get_str());
                if (words.find(key) != words.end()) {
                    typefilter = key;
                    continue;
                }
            }
            if (countryfilter.empty()) {
                if (countryiso.isValid(params[i].get_str())) {
                    countryfilter = params[i].get_str();
                    continue;
                }
            }
            if (currencyfilter.empty()) {
                if (currencyiso.isValid(params[i].get_str())) {
                    currencyfilter = params[i].get_str();
                    continue;
                }
            }
            if (methodfilter.empty()) {
                std::string methodname = boost::algorithm::to_lower_copy(params[i].get_str());
                std::list<dex::PaymentMethodInfo> pms = dex::DexDB::self()->getPaymentMethodsInfo();
                for (auto j : pms) {
                    std::string name = boost::algorithm::to_lower_copy(j.name);
                    if (name == methodname) {
                        methodfilter = j.name;
                        methodfiltertype = j.type;
                        continue;
                    }
                }
            }
            if (statusfilter == dex::Indefined) {
                status.set(params[i].get_str());
                if (status != dex::Indefined) {
                    statusfilter = status;
                }
            }
        }
    }

    if (typefilter.empty()) {
        typefilter = "all";
    }

    // check country and currency in DB
    if (countryfilter != "") {
        dex::CountryInfo  countryinfo = dex::DexDB::self()->getCountryInfo(countryfilter);
        if (!countryinfo.enabled) {
            throw runtime_error("\nERROR: this country is disabled in DB\n");
        }
    }
    if (currencyfilter != "") {
        dex::CurrencyInfo  currencyinfo = dex::DexDB::self()->getCurrencyInfo(currencyfilter);
        if (!currencyinfo.enabled) {
            throw runtime_error("\nERROR: this currency is disabled in DB\n");
        }
    }

    if (limit == 0) {
        limit = maxOutput();
    }

    std::list<dex::MyOfferInfo> myoffers = dex::DexDB::self()->getMyOffers(countryfilter, currencyfilter, methodfiltertype, words[typefilter], status, limit, offset);

    for (auto i : myoffers) {
        CDexOffer o(i.getOfferInfo(), i.type);
        UniValue v = o.getUniValue();
        v.push_back(Pair("status", i.status));
        v.push_back(Pair("statusStr", status.status2str(i.status)));
        result.push_back(v);
    }

    return result;
}




UniValue deldexoffer(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }
    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "deldexoffer <hash>\n\n"
            "Delete offer from local DB and broadcast message.\n"
            "To do this, you need a private key in a wallet that matches the public key in the offer.\n"

            "\nArgument:\n"
            "\thash         (string) offer hash, hex digest.\n"

            "\nExample:\n"
            + HelpExampleCli("deldexoffer", "AABB...CCDD")
        );

    std::string strOfferHash = params[0].get_str();
    if (strOfferHash.empty()) {
        throw runtime_error("\nERROR: offer hash is empty");
    }

    uint256 hash = uint256S(strOfferHash);
    if (hash.IsNull()) {
        throw runtime_error("\nERROR: offer hash error\n");
    }

    //dex::DexDB db(strDexDbFile);

    CDexOffer offer;
    if (dex::DexDB::self()->isExistMyOfferByHash(hash)) {
        dex::MyOfferInfo myoffer = dex::DexDB::self()->getMyOfferByHash(hash);
        offer = CDexOffer(myoffer);
    } else if (dex::DexDB::self()->isExistOfferBuyByHash(hash)) {
        offer = CDexOffer(dex::DexDB::self()->getOfferBuyByHash(hash), dex::Buy);
    } else if (dex::DexDB::self()->isExistOfferSellByHash(hash)) {
        offer = CDexOffer(dex::DexDB::self()->getOfferSellByHash(hash), dex::Sell);
    } else {
        throw runtime_error("\nERROR: offer not found in DB\n");
    }

    CDex dex(offer);
    std::string error;
    CKey key;
    if (!dex.FindKey(key, error)) {
        throw runtime_error(error.c_str());
    }

    std::vector<unsigned char> vchSign;
    if (!dex.SignOffer(key, vchSign, error)) {
        throw runtime_error(error.c_str());
    }

    int sended = 0;
    if (offer.status != dex::Draft) {
        auto vNodesCopy = CopyNodeVector();
        for (auto pNode : vNodesCopy) {
            uint64_t bytes = pNode->nSendBytes;
            pNode->PushMessage(NetMsgType::DEXDELOFFER, offer, vchSign);
            if (pNode->nSendBytes > bytes) sended++;
        }

        ReleaseNodeVector(vNodesCopy);
    }

    if (sended > 1 || offer.status == dex::Draft || offer.status == dex::Indefined) {
        if (offer.isBuy()  && offer.status != dex::Draft) dex::DexDB::self()->deleteOfferBuyByHash(offer.hash, false);
        if (offer.isSell() && offer.status != dex::Draft) dex::DexDB::self()->deleteOfferSellByHash(offer.hash, false);
        if (offer.isMyOffer()) dex::DexDB::self()->deleteMyOfferByHash(offer.hash, false);
    }

    throw runtime_error("\nsuccess\n");

    return NullUniValue;
}



UniValue adddexoffer(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }
    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }
    if (fHelp || params.size() != 1)
        throw runtime_error(

            "adddexoffer <json-data>\n\n"

            "\nArgument:\n"
            "\tjson-data    (string) offer data in format json.\n"

            "\njson attributes:\n"
            "\ttype             (string) offer type, 'buy' or 'sell'\n"
            "\tcountryIso       (string) two-letter country code (ISO 3166-1 alpha-2 code)\n"
            "\tcurrencyIso      (string) three-letter currency code (ISO 4217)\n"
            "\tpaymentMethod    (number) payment method, correct values: 1(cash payment), 128(online payment)\n"
            "\tprice            (string) offer price, max 8 digits after the decimal point\n"
            "\tminAmount        (string) offer minAmount, max 8 digits after the decimal point\n"
            "\ttimeToExpiration (number) period valid offer, correct values: 10, 20, 30\n"
            "\tshortInfo        (string) short info, max 140 symbols\n"
            "\tdetails          (string) detail info\n"

            "\nExample:\n"
            + HelpExampleCli("adddexoffer", "\"{"
                                            "\\\"type\\\": \\\"sell\\\","
                                            "\\\"countryIso\\\": \\\"RU\\\","
                                            "\\\"currencyIso\\\": \\\"RUB\\\","
                                            "\\\"paymentMethod\\\": 1,"
                                            "\\\"price\\\": \\\"100.05\\\","
                                            "\\\"minAmount\\\": \\\"10.005\\\","
                                            "\\\"timeToExpiration\\\": 30,"
                                            "\\\"shortInfo\\\": \\\"test offer\\\","
                                            "\\\"details\\\": \\\"test offer details\\\""
                                            "}\"")
        );

    std::string jsonData = params[0].get_str();
    std::string error;

    MyOfferInfo offer = jsonToMyOfferInfo(jsonData, error);
    offer.status = Draft;
    offer.editingVersion = 0;

    if (error.length() > 0) {
        throw runtime_error("\nERROR: " + error);
    }

    CDexOffer cOffer;
    CKey key = pwalletMain->GeneratePrivKey();
    CPubKey pkey = key.GetPubKey();
    if (!pwalletMain->AddKeyPubKey(key, pkey)) {
        throw runtime_error("\nERROR: add key to wallet error");
    }

    offer.pubKey = HexStr(pkey.begin(), pkey.end());

    if (!cOffer.Create(offer)) {
        throw runtime_error("\nERROR: error create offer");
    }

    CallBackDBForRpc callBack;
    dex::DexDB::self()->addCallBack(&callBack);
    dex::DexDB::self()->addMyOffer(MyOfferInfo(cOffer), false);
    CallBackStatus status = callBack.statusAddMyOffer();
    dex::DexDB::self()->removeCallBack(&callBack);

    if (status == CallBackStatus::Error && dex::DexDB::self()->isExistMyOfferByHash(cOffer.hash)) {
        throw runtime_error("\nERROR: the operation failed");
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hash", cOffer.hash.GetHex()));
    return result;
}

UniValue editdexoffer(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }
    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }
    if (fHelp || params.size() != 2)
        throw runtime_error(

            "editdexoffer <hash> <json-data>\n\n"

            "\nArgument:\n"
            "\thash         (string) offer hash, hex digest.\n"
            "\tjson-data    (string) offer data in format json.\n"

            "\nWARNING: If offer have status Active, you can change only price, shortInfo, details"

            "\njson attributes:\n"
            "\ttype             (string) offer type, 'buy' or 'sell'\n"
            "\tcountryIso       (string) two-letter country code (ISO 3166-1 alpha-2 code)\n"
            "\tcurrencyIso      (string) three-letter currency code (ISO 4217)\n"
            "\tpaymentMethod    (number) payment method, correct values: 1(cash payment), 128(online payment)\n"
            "\tprice            (string) offer price, max 8 digits after the decimal point\n"
            "\tminAmount        (string) offer minAmount, max 8 digits after the decimal point\n"
            "\ttimeToExpiration (number) period valid offer, correct values: 10, 20, 30\n"
            "\tshortInfo        (string) short info, max 140 symbols\n"
            "\tdetails          (string) detail info\n"

            "\nExample:\n"
            + HelpExampleCli("editdexoffer", "AABB...CCDD \"{"
                                            "\\\"type\\\": \\\"sell\\\","
                                            "\\\"countryIso\\\": \\\"RU\\\","
                                            "\\\"currencyIso\\\": \\\"RUB\\\","
                                            "\\\"paymentMethod\\\": 1,"
                                            "\\\"price\\\": \\\"100.03\\\","
                                            "\\\"minAmount\\\": \\\"10.005\\\","
                                            "\\\"timeToExpiration\\\": 30,"
                                            "\\\"shortInfo\\\": \\\"test offer\\\","
                                            "\\\"details\\\": \\\"test offer details\\\""
                                            "}\"")
        );

    std::string strOfferHash = params[0].get_str();
    if (strOfferHash.empty()) {
        throw runtime_error("\nERROR: offer hash is empty");
    }

    uint256 hash = uint256S(strOfferHash);
    if (hash.IsNull()) {
        throw runtime_error("\nERROR: offer hash error\n");
    }

    if (!dex::DexDB::self()->isExistMyOfferByHash(hash)) {
        throw runtime_error("\nERROR: offer not found in DB\n");
    }

    std::string jsonData = params[1].get_str();
    std::string error;
    MyOfferInfo offer = jsonToMyOfferInfo(jsonData, error);

    if (error.length() > 0) {
        throw runtime_error("\nERROR: " + error);
    }

    MyOfferInfo currentMyOffer = dex::DexDB::self()->getMyOfferByHash(hash);
    if (currentMyOffer.status == Draft) {
        offer.status = Draft;
        offer.editingVersion = 0;

        CallBackDBForRpc callBack;
        dex::DexDB::self()->addCallBack(&callBack);

        dexman.addOrEditDraftMyOffer(offer, false);
        CallBackStatus status = callBack.statusChangedMyOffer();
        dex::DexDB::self()->removeCallBack(&callBack);
        if (status == CallBackStatus::Error && dex::DexDB::self()->isExistMyOfferByHash(offer.hash)) {
            throw runtime_error("\nERROR: the operation failed");
        }

        UniValue result(UniValue::VOBJ);
        result.push_back(Pair("new hash", offer.hash.GetHex()));
        return result;
    } else if (currentMyOffer.status == Active) {
        if (currentMyOffer.type != offer.type) {
            throw runtime_error("\nERROR: unchanged data doesn't match");
        }

        if (currentMyOffer.countryIso != offer.countryIso) {
            throw runtime_error("\nERROR: unchanged data doesn't match");
        }

        if (currentMyOffer.currencyIso != offer.currencyIso) {
            throw runtime_error("\nERROR: unchanged data doesn't match");
        }

        if (currentMyOffer.paymentMethod != offer.paymentMethod) {
            throw runtime_error("\nERROR: unchanged data doesn't match");
        }

        if (currentMyOffer.minAmount != offer.minAmount) {
            throw runtime_error("\nERROR: unchanged data doesn't match");
        }

        int shelfLife = ((offer.timeToExpiration - offer.timeCreate - 1) / 86400) +1;
        int currentShelfLife = ((currentMyOffer.timeToExpiration - currentMyOffer.timeCreate - 1) / 86400) +1;

        if (shelfLife != currentShelfLife) {
            throw runtime_error("\nERROR: unchanged data doesn't match");
        }

        currentMyOffer.price = offer.price;
        currentMyOffer.shortInfo = offer.shortInfo;
        currentMyOffer.details = offer.details;

        CallBackDBForRpc callBack;
        dex::DexDB::self()->addCallBack(&callBack);

        std::string error;
        dexman.prepareAndSendMyOffer(currentMyOffer, error, false);

        CallBackStatus status = callBack.statusChangedMyOffer();
        dex::DexDB::self()->removeCallBack(&callBack);
        if (status == CallBackStatus::Error && dex::DexDB::self()->isExistMyOfferByHash(currentMyOffer.hash)) {
            throw runtime_error("\nERROR: the operation failed");
        }

        if (!error.empty()) {
            throw runtime_error("\nERROR: " + error + "\n");
        }
    }

    return NullUniValue;
}

UniValue senddexoffer(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp || params.size() != 1)
        throw runtime_error(

            "senddexoffer <hash>\n\n"

            "\nArgument:\n"
            "\thash    (string) offer hash, hex digest.\n"

            "\nExample:\n"
            + HelpExampleCli("senddexoffer", "AABB...CCDD")
        );

    std::string strOfferHash = params[0].get_str();
    if (strOfferHash.empty()) {
        throw runtime_error("\nERROR: offer hash is empty");
    }

    uint256 hash = uint256S(strOfferHash);
    if (hash.IsNull()) {
        throw runtime_error("\nERROR: offer hash error\n");
    }

    if (!dex::DexDB::self()->isExistMyOfferByHash(hash)) {
        throw runtime_error("\nERROR: offer not found in DB\n");
    }

    MyOfferInfo myOffer = dex::DexDB::self()->getMyOfferByHash(hash);

    std::string error;
    //myOffer.timeCreate = GetTime(); error with change hash!!!!
    dexman.prepareAndSendMyOffer(myOffer, error, false);

    if (!error.empty()) {
        throw runtime_error("\nERROR: " + error + "\n");
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("new hash", myOffer.hash.GetHex()));
    return result;
}

UniValue dexsync(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp || params.size() != 1)
        throw runtime_error(
                "dexsync [status|reset]\n"
                "if status that returns status synchronization dex\n"

                "\nExample:\n"
                + HelpExampleCli("dexsync", "status")
                );

    UniValue result(UniValue::VOBJ);

    std::string key = params[0].get_str();
    if (key == "status") {
        auto status =  dex::dexsync.getSyncStatus();
        result.push_back(Pair("status", status));
    } else if (key == "reset") {
        if (dex::dexsync.reset()) {
           result.push_back(Pair("status", "reset sunc"));
        } else {
           result.push_back(Pair("status", "reset is not available now"));
        }
    } else if (key == "force-synced") {
        dex::dexsync.forceSynced();
        result.push_back(Pair("status", "force synced"));
    } else {
        throw runtime_error("\nwrong parameter " + key + "\n");
    }

    return result;
}

UniValue dexsettings(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "dexsettings [maxoutput num]\n"
                "maxoutput return max number output offer dex\n"
                "num - (number, optional) if num not empty changed max number output, if num == 0 show all"

                "\nExample:\n"
                + HelpExampleCli("dexsettings", "maxoutput 100")
                );

    UniValue result(UniValue::VOBJ);

    std::string key = params[0].get_str();
    if (key == "maxoutput") {
        int num;
        if (params.size() == 2) {
            num = params[1].get_int();
            changedMaxOutput(num);
        } else {
            num = maxOutput();
        }

        if (num == 0) {
            result.push_back(Pair("maxoutput", "all"));
        } else {
            result.push_back(Pair("maxoutput", num));
        }
    } else {
        throw runtime_error("\nwrong parameter " + key + "\n");
    }

    return result;
}



UniValue getdexinfo(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp)
        throw runtime_error(
            "getdexinfo\n"
            "Return short info about offers count in DB."
        );


    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("offersSell", static_cast<uint64_t>(dex::DexDB::self()->countOffersSell())));
    result.push_back(Pair("offersBuy", static_cast<uint64_t>(dex::DexDB::self()->countOffersBuy())));
    result.push_back(Pair("myOffers", static_cast<uint64_t>(dex::DexDB::self()->countMyOffers())));
    result.push_back(Pair("uncOffers", static_cast<uint64_t>(dexman.getUncOffers()->getSize())));
    return result;
}

UniValue dexunconfirmed(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp)
        throw runtime_error(
            "dexunconfirmed\n"
            "Return list pair{hash, idTransaction} unconfirmed offers.\n"
        );

    UniValue result(UniValue::VARR);

    auto unc = dexman.getUncOffers()->getAllOffers();
    for (auto i : unc) {
        UniValue v(UniValue::VOBJ);
        v.push_back(Pair("hash", i.hash.GetHex()));
        v.push_back(Pair("txid", i.idTransaction.GetHex()));
        result.push_back(v);
    }

    return result;
}

UniValue getdexoffer(const UniValue& params, bool fHelp)
{
    if (!fTxIndex) {
        throw runtime_error(
            "To use this feture please enable -txindex and make -reindex.\n"
        );
    }

    if (dex::DexDB::self() == nullptr) {
        throw runtime_error(
            "DexDB is not initialized.\n"
        );
    }

    if (fHelp)
        throw runtime_error(
            "getdexoffer <hash>\n"
            "Return detail info about offer.\n"

            "nResult (for example):\n"
            "[\n"
            "   {\n"
            "     \"type\"          : \"sell\",   offer type, buy or sell\n"
            "     \"idTransaction\" : \"<id>\",   transaction with offer fee\n"
            "     \"hash\"          : \"<hash>\", offer hash\n"
            "     \"pubKey\"        : \"<key>\",  offer public key\n"
            "     \"countryIso\"    : \"RU\",     country (ISO 3166-1 alpha-2)\n"
            "     \"currencyIso\"   : \"RUB\",    currency (ISO 4217)\n"
            "     \"paymentMethod\" : 1,        payment method code (default 1 - cash, 128 - online)\n"
            "     \"price\"         : 10000,\n"
            "     \"minAmount\"     : 1000,\n"
            "     \"timeCreate\"    : 947...9344,\n"
            "     \"timeExpiration\": 947...5344, offer expiration\n"
            "     \"shortInfo\"     : \"...\",    offer short info (max 140 bytes)\n"
            "     \"details\"       : \"...\"     offer details (max 1024 bytes)\n"
            "   },\n"
            "   ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("getdexoffer", "AABB...CCDD")
        );

    std::string strOfferHash = params[0].get_str();
    if (strOfferHash.empty()) {
        throw runtime_error("\nERROR: offer hash is empty");
    }

    uint256 hash = uint256S(strOfferHash);
    if (hash.IsNull()) {
        throw runtime_error("\nERROR: offer hash error\n");
    }

    CDexOffer offer;
    if (dex::DexDB::self()->isExistOfferSellByHash(hash)) {
        auto info = dex::DexDB::self()->getOfferSellByHash(hash);
        offer = CDexOffer(info, dex::TypeOffer::Sell);
    } else if (dex::DexDB::self()->isExistOfferBuyByHash(hash)) {
        auto info = dex::DexDB::self()->getOfferBuyByHash(hash);
        offer = CDexOffer(info, dex::TypeOffer::Buy);
    } else {
        offer = dexman.getUncOffers()->getOfferByHash(hash);
    }

    if (offer.IsNull()) {
        UniValue result(UniValue::VOBJ);
        return result;
    } else {
        return offer.getUniValue();
    }
}
