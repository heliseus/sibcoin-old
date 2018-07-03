#include <boost/test/unit_test.hpp>
#include <stdio.h>
#include "test/test_sibcoin.h"

#include "dex/db/dexdb.h"
#include "dex/db/callbackdb.h"
#include "random.h"
#include "util.h"

using namespace dex;

class CallBackOffers : public CallBackDB
{
public:
    CallBackOffers() {
    }

    virtual void finishTableOperation(const TypeTable &table, const TypeTableOperation &operation, const StatusTableOperation &status) {
        this->table = table;
        this->operation = operation;
        this->status = status;
    }

    TypeTable getTypeTable() {
        return table;
    }

    TypeTableOperation getTypeTableOperation() {
        return operation;
    }

    StatusTableOperation getStatusTableOperation() {
        return status;
    }

private:
    TypeTable table;
    TypeTableOperation operation;
    StatusTableOperation status;
};

void checkCountry(DexDB *db)
{
    auto cList = db->getCountriesInfo();
    int size = cList.size();
    BOOST_CHECK(size == 247);

    db->deleteCountry("RU");
    db->deleteCountry("US");

    cList = db->getCountriesInfo();
    size = cList.size();

    BOOST_CHECK(size == 245);

    auto front = cList.front();
    cList.pop_front();
    cList.push_back(front);

    db->editCountries(cList);
    cList = db->getCountriesInfo();
    auto back = cList.back();

    BOOST_CHECK(front.name == back.name && front.iso == back.iso && front.enabled == back.enabled);

    auto find = db->getCountryInfo(front.iso);

    BOOST_CHECK(find.name == front.name && find.iso == front.iso && find.enabled == front.enabled);
}

void checkCurrency(DexDB *db)
{
    auto cList = db->getCurrenciesInfo();
    int size = cList.size();
    BOOST_CHECK(size == 147);

    db->deleteCurrency("RUB");
    db->deleteCurrency("USD");

    cList = db->getCurrenciesInfo();
    size = cList.size();

    BOOST_CHECK(size == 145);

    auto front = cList.front();
    cList.pop_front();
    cList.push_back(front);

    db->editCurrencies(cList);
    cList = db->getCurrenciesInfo();
    auto back = cList.back();

    BOOST_CHECK(front.name == back.name && front.iso == back.iso && front.symbol == back.symbol && front.enabled == back.enabled);

    auto find = db->getCurrencyInfo(front.iso);

    BOOST_CHECK(find.name == front.name && find.iso == front.iso && find.symbol == front.symbol && find.enabled == front.enabled);
}

void checkPaymentMethod(DexDB *db)
{
    auto cList = db->getPaymentMethodsInfo();
    int size = cList.size();
    BOOST_CHECK(size == 2);

    auto front = cList.front();
    auto find = db->getPaymentMethodInfo(front.type);

    BOOST_CHECK(find.name == front.name && find.type == front.type && find.description == front.description);

    db->deletePaymentMethod(1);
    db->deletePaymentMethod(128);

    cList = db->getPaymentMethodsInfo();
    size = cList.size();

    BOOST_CHECK(size == 0);
}

void checkOffers(DexDB *db)
{
    long int currentTime = static_cast<long int>(time(NULL));
    int secInDay = 86400;
    long int lastMod = currentTime + secInDay;

    OfferInfo info;
    info.pubKey = GetRandHash().GetHex();
    info.hash = GetRandHash();
    info.idTransaction = GetRandHash();
    info.price = 1234567;
    info.minAmount = 10000;
    info.shortInfo = "first info";
    info.countryIso = "RU";
    info.currencyIso = "RUB";
    info.paymentMethod = 1;
    info.timeCreate = currentTime - secInDay * 10;
    info.timeToExpiration = info.timeCreate + secInDay;
    info.timeModification = lastMod;
    info.editingVersion = 0;

    std::list<OfferInfo> iList;
    iList.push_back(info);
    db->addOfferBuy(info);
    db->addOfferSell(info);

    info.pubKey = GetRandHash().GetHex();
    info.hash = GetRandHash();
    info.idTransaction = GetRandHash();
    info.price = 1555;
    info.minAmount = 0;
    info.shortInfo = "info";
    info.countryIso = "US";
    info.currencyIso = "USD";
    info.paymentMethod = 128;
    info.timeCreate = currentTime - secInDay * 20;
    info.timeToExpiration = info.timeCreate + secInDay * 5;
    info.timeModification = lastMod - secInDay * 3;
    info.editingVersion = 2;

    iList.push_back(info);
    db->addOfferBuy(info);
    db->addOfferSell(info);

    info.pubKey = GetRandHash().GetHex();
    info.hash = GetRandHash();
    info.idTransaction = GetRandHash();
    info.price = 133;
    info.minAmount = 3;
    info.shortInfo = "info";
    info.countryIso = "UA";
    info.currencyIso = "UAN";
    info.paymentMethod = 128;
    info.timeCreate = currentTime - secInDay * 13;
    info.timeToExpiration = info.timeCreate + secInDay * 4;
    info.timeModification = lastMod - secInDay * 4;
    info.editingVersion = 3;

    iList.push_back(info);
    db->addOfferBuy(info);
    db->addOfferSell(info);

    BOOST_CHECK(db->isExistOfferBuy(iList.front().idTransaction));
    BOOST_CHECK(db->isExistOfferSell(iList.back().idTransaction));
    BOOST_CHECK(db->isExistOfferBuyByHash(iList.front().hash));
    BOOST_CHECK(db->isExistOfferSellByHash(iList.back().hash));

    BOOST_CHECK(!db->isExistOfferBuy(GetRandHash()));
    BOOST_CHECK(!db->isExistOfferSell(GetRandHash()));
    BOOST_CHECK(!db->isExistOfferBuyByHash(GetRandHash()));
    BOOST_CHECK(!db->isExistOfferSellByHash(GetRandHash()));

    BOOST_CHECK(db->countOffersBuy() == iList.size());
    BOOST_CHECK(db->countOffersSell() == iList.size());

    BOOST_CHECK(db->countOffersBuy("RU", "RUB", 1) == 1);
    BOOST_CHECK(db->countOffersSell("RU", "RUB", 1) == 1);
    BOOST_CHECK(db->countOffersBuy("US", "RUB", 1) == 0);
    BOOST_CHECK(db->countOffersSell("US", "RUB", 1) == 0);

    BOOST_CHECK(db->lastModificationOffersBuy() == lastMod);
    BOOST_CHECK(db->lastModificationOffersSell() == lastMod);

    BOOST_CHECK(db->getHashsAndEditingVersionsBuy().size() == 3);
    BOOST_CHECK(db->getHashsAndEditingVersionsSell().size() == 3);

    BOOST_CHECK(db->getHashsAndEditingVersionsBuy(DexDB::OffersPeriod::Before, lastMod - secInDay * 30).size() == 0);
    BOOST_CHECK(db->getHashsAndEditingVersionsSell(DexDB::OffersPeriod::Before, lastMod - secInDay * 30).size() == 0);
    BOOST_CHECK(db->getHashsAndEditingVersionsBuy(DexDB::OffersPeriod::Before, lastMod).size() == 2);
    BOOST_CHECK(db->getHashsAndEditingVersionsSell(DexDB::OffersPeriod::Before, lastMod).size() == 2);

    BOOST_CHECK(db->getHashsAndEditingVersionsBuy(DexDB::OffersPeriod::After, lastMod).size() == 1);
    BOOST_CHECK(db->getHashsAndEditingVersionsSell(DexDB::OffersPeriod::After, lastMod).size() == 1);
    BOOST_CHECK(db->getHashsAndEditingVersionsBuy(DexDB::OffersPeriod::After, lastMod + 1).size() == 0);
    BOOST_CHECK(db->getHashsAndEditingVersionsSell(DexDB::OffersPeriod::After, lastMod + 1).size() == 0);

    BOOST_CHECK(db->countOffersBuy(DexDB::OffersPeriod::Before, lastMod - secInDay * 30) == 0);
    BOOST_CHECK(db->countOffersSell(DexDB::OffersPeriod::Before, lastMod - secInDay * 30) == 0);
    BOOST_CHECK(db->countOffersBuy(DexDB::OffersPeriod::Before, lastMod) == 2);
    BOOST_CHECK(db->countOffersSell(DexDB::OffersPeriod::Before, lastMod) == 2);

    BOOST_CHECK(db->countOffersBuy(DexDB::OffersPeriod::After, lastMod) == 1);
    BOOST_CHECK(db->countOffersSell(DexDB::OffersPeriod::After, lastMod) == 1);
    BOOST_CHECK(db->countOffersBuy(DexDB::OffersPeriod::After, lastMod + 1) == 0);
    BOOST_CHECK(db->countOffersSell(DexDB::OffersPeriod::After, lastMod + 1) == 0);

    db->deleteOfferSell(iList.front().idTransaction);
    db->deleteOfferBuy(iList.back().idTransaction);

    BOOST_CHECK(!db->isExistOfferSell(iList.front().idTransaction));
    BOOST_CHECK(!db->isExistOfferBuy(iList.back().idTransaction));

    OfferInfo info1 = iList.back();
    info1.price = 133;
    info1.minAmount = 3;
    info1.shortInfo = "info test";
    info1.countryIso = "AF";
    info1.currencyIso = "AFN";
    info1.paymentMethod = 200;
    info1.timeCreate = currentTime - secInDay * 33;
    info1.timeToExpiration = info1.timeCreate + secInDay * 20;
    info1.timeModification = lastMod - secInDay * 20;
    info1.editingVersion = 4;

    db->editOfferSell(info1);

    OfferInfo info2 = iList.front();
    info2.price = 4444;
    info2.minAmount = 444;
    info2.shortInfo = "info test 4";
    info2.countryIso = "AX";
    info2.currencyIso = "EUR";
    info2.paymentMethod = 150;
    info2.timeCreate = currentTime - secInDay * 7;
    info2.timeToExpiration = info2.timeCreate + secInDay * 2;
    info2.timeModification = lastMod - secInDay * 3;
    info2.editingVersion = 6;

    db->editOfferBuy(info2);

    OfferInfo sell = db->getOfferSell(info1.idTransaction);

    BOOST_CHECK(sell.pubKey == info1.pubKey);
    BOOST_CHECK(sell.idTransaction == info1.idTransaction);
    BOOST_CHECK(sell.hash == info1.hash);
    BOOST_CHECK(sell.price == info1.price);
    BOOST_CHECK(sell.minAmount == info1.minAmount);
    BOOST_CHECK(sell.shortInfo == info1.shortInfo);
    BOOST_CHECK(sell.countryIso == info1.countryIso);
    BOOST_CHECK(sell.currencyIso == info1.currencyIso);
    BOOST_CHECK(sell.paymentMethod == info1.paymentMethod);
    BOOST_CHECK(sell.timeCreate == info1.timeCreate);
    BOOST_CHECK(sell.timeToExpiration == info1.timeToExpiration);
    BOOST_CHECK(sell.timeModification == info1.timeModification);
    BOOST_CHECK(sell.editingVersion == info1.editingVersion);

    sell = db->getOfferSellByHash(info1.hash);

    BOOST_CHECK(sell.pubKey == info1.pubKey);
    BOOST_CHECK(sell.idTransaction == info1.idTransaction);
    BOOST_CHECK(sell.hash == info1.hash);
    BOOST_CHECK(sell.price == info1.price);
    BOOST_CHECK(sell.minAmount == info1.minAmount);
    BOOST_CHECK(sell.shortInfo == info1.shortInfo);
    BOOST_CHECK(sell.countryIso == info1.countryIso);
    BOOST_CHECK(sell.currencyIso == info1.currencyIso);
    BOOST_CHECK(sell.paymentMethod == info1.paymentMethod);
    BOOST_CHECK(sell.timeCreate == info1.timeCreate);
    BOOST_CHECK(sell.timeToExpiration == info1.timeToExpiration);
    BOOST_CHECK(sell.timeModification == info1.timeModification);
    BOOST_CHECK(sell.editingVersion == info1.editingVersion);

    OfferInfo buy = db->getOfferBuy(info2.idTransaction);

    BOOST_CHECK(buy.pubKey == info2.pubKey);
    BOOST_CHECK(buy.idTransaction == info2.idTransaction);
    BOOST_CHECK(buy.hash == info2.hash);
    BOOST_CHECK(buy.price == info2.price);
    BOOST_CHECK(buy.minAmount == info2.minAmount);
    BOOST_CHECK(buy.shortInfo == info2.shortInfo);
    BOOST_CHECK(buy.countryIso == info2.countryIso);
    BOOST_CHECK(buy.currencyIso == info2.currencyIso);
    BOOST_CHECK(buy.paymentMethod == info2.paymentMethod);
    BOOST_CHECK(buy.timeCreate == info2.timeCreate);
    BOOST_CHECK(buy.timeToExpiration == info2.timeToExpiration);
    BOOST_CHECK(buy.timeModification == info2.timeModification);
    BOOST_CHECK(buy.editingVersion == info2.editingVersion);

    buy = db->getOfferBuyByHash(info2.hash);

    BOOST_CHECK(buy.pubKey == info2.pubKey);
    BOOST_CHECK(buy.idTransaction == info2.idTransaction);
    BOOST_CHECK(buy.hash == info2.hash);
    BOOST_CHECK(buy.price == info2.price);
    BOOST_CHECK(buy.minAmount == info2.minAmount);
    BOOST_CHECK(buy.shortInfo == info2.shortInfo);
    BOOST_CHECK(buy.countryIso == info2.countryIso);
    BOOST_CHECK(buy.currencyIso == info2.currencyIso);
    BOOST_CHECK(buy.paymentMethod == info2.paymentMethod);
    BOOST_CHECK(buy.timeCreate == info2.timeCreate);
    BOOST_CHECK(buy.timeToExpiration == info2.timeToExpiration);
    BOOST_CHECK(buy.timeModification == info2.timeModification);
    BOOST_CHECK(buy.editingVersion == info2.editingVersion);

    auto list = db->getOffersSell();

    for (auto item : list) {
        OfferInfo sell = db->getOfferSell(item.idTransaction);

        BOOST_CHECK(sell.pubKey == item.pubKey);
        BOOST_CHECK(sell.idTransaction == item.idTransaction);
        BOOST_CHECK(sell.hash == item.hash);
        BOOST_CHECK(sell.price == item.price);
        BOOST_CHECK(sell.minAmount == item.minAmount);
        BOOST_CHECK(sell.shortInfo == item.shortInfo);
        BOOST_CHECK(sell.countryIso == item.countryIso);
        BOOST_CHECK(sell.currencyIso == item.currencyIso);
        BOOST_CHECK(sell.paymentMethod == item.paymentMethod);
        BOOST_CHECK(sell.timeCreate == item.timeCreate);
        BOOST_CHECK(sell.timeToExpiration == item.timeToExpiration);
        BOOST_CHECK(sell.timeModification == item.timeModification);
        BOOST_CHECK(sell.editingVersion == item.editingVersion);
    }

    list = db->getOffersBuy();

    for (auto item : list) {
        OfferInfo buy = db->getOfferBuy(item.idTransaction);

        BOOST_CHECK(buy.pubKey == item.pubKey);
        BOOST_CHECK(buy.idTransaction == item.idTransaction);
        BOOST_CHECK(buy.hash == item.hash);
        BOOST_CHECK(buy.price == item.price);
        BOOST_CHECK(buy.minAmount == item.minAmount);
        BOOST_CHECK(buy.shortInfo == item.shortInfo);
        BOOST_CHECK(buy.countryIso == item.countryIso);
        BOOST_CHECK(buy.currencyIso == item.currencyIso);
        BOOST_CHECK(buy.paymentMethod == item.paymentMethod);
        BOOST_CHECK(buy.timeCreate == item.timeCreate);
        BOOST_CHECK(buy.timeToExpiration == item.timeToExpiration);
        BOOST_CHECK(buy.timeModification == item.timeModification);
        BOOST_CHECK(buy.editingVersion == item.editingVersion);
    }

    db->deleteOldOffersSell();
    db->deleteOldOffersBuy();

    list = db->getOffersSell();

    BOOST_CHECK(list.empty());

    list = db->getOffersBuy();

    BOOST_CHECK(list.empty());
}

void checkMyOffers(DexDB *db)
{
    long int currentTime = static_cast<long int>(time(NULL));
    int secInDay = 86400;

    MyOfferInfo info;
    info.pubKey = GetRandHash().GetHex();
    info.hash = GetRandHash();
    info.idTransaction = GetRandHash();
    info.type = Buy;
    info.status = Active;
    info.price = 1234567;
    info.minAmount = 10000;
    info.shortInfo = "first info";
    info.countryIso = "RU";
    info.currencyIso = "RUB";
    info.paymentMethod = 1;
    info.timeCreate = currentTime - secInDay * 15;
    info.timeToExpiration = info.timeCreate + secInDay * 5;
    info.timeModification = info.timeCreate;
    info.editingVersion = 0;

    std::list<MyOfferInfo> iList;
    iList.push_back(info);
    db->addMyOffer(info);

    info.pubKey = GetRandHash().GetHex();
    info.hash = GetRandHash();
    info.idTransaction = GetRandHash();
    info.type = Buy;
    info.status = Draft;
    info.price = 1555;
    info.minAmount = 0;
    info.shortInfo = "info";
    info.countryIso = "US";
    info.currencyIso = "USD";
    info.paymentMethod = 128;
    info.timeCreate = currentTime - secInDay * 33;
    info.timeToExpiration = info.timeCreate + secInDay * 18;
    info.timeModification = info.timeCreate;
    info.editingVersion = 4;

    iList.push_back(info);
    db->addMyOffer(info);

    info.pubKey = GetRandHash().GetHex();
    info.hash = GetRandHash();
    info.idTransaction = GetRandHash();
    info.type = Sell;
    info.status = Active;
    info.price = 133;
    info.minAmount = 3;
    info.shortInfo = "info";
    info.countryIso = "UA";
    info.currencyIso = "UAN";
    info.paymentMethod = 128;
    info.timeCreate = currentTime - secInDay * 17;
    info.timeToExpiration = info.timeCreate + secInDay * 8;
    info.timeModification = info.timeCreate;
    info.editingVersion = 6;

    iList.push_back(info);
    db->addMyOffer(info);

    BOOST_CHECK(db->isExistMyOffer(iList.front().idTransaction));
    BOOST_CHECK(db->isExistMyOffer(iList.back().idTransaction));
    BOOST_CHECK(db->isExistMyOfferByHash(iList.front().hash));
    BOOST_CHECK(db->isExistMyOfferByHash(iList.back().hash));

    BOOST_CHECK(!db->isExistMyOffer(GetRandHash()));
    BOOST_CHECK(!db->isExistMyOffer(GetRandHash()));
    BOOST_CHECK(!db->isExistMyOfferByHash(GetRandHash()));
    BOOST_CHECK(!db->isExistMyOfferByHash(GetRandHash()));

    BOOST_CHECK(db->countMyOffers() == iList.size());
    BOOST_CHECK(db->countMyOffers("RU", "RUB", 1, Buy, Active) == 1);
    BOOST_CHECK(db->countMyOffers("RU", "RUB", 1, Buy, Draft) == 0);

    db->deleteMyOffer(iList.front().idTransaction);

    BOOST_CHECK(!db->isExistMyOffer(iList.front().idTransaction));

    MyOfferInfo info1 = iList.back();
    info.type = Buy;
    info.status = Expired;
    info1.price = 133;
    info1.minAmount = 3;
    info1.shortInfo = "info test";
    info1.countryIso = "AF";
    info1.currencyIso = "AFN";
    info1.paymentMethod = 77;
    info1.timeCreate = currentTime - secInDay * 7;
    info1.timeToExpiration = info1.timeCreate + secInDay * 2;
    info1.timeModification = currentTime - secInDay * 3;
    info1.editingVersion = 7;

    db->editMyOffer(info1);

    MyOfferInfo offer = db->getMyOffer(info1.idTransaction);

    BOOST_CHECK(offer.pubKey == info1.pubKey);
    BOOST_CHECK(offer.idTransaction == info1.idTransaction);
    BOOST_CHECK(offer.hash == info1.hash);
    BOOST_CHECK(offer.type == info1.type);
    BOOST_CHECK(offer.status == info1.status);
    BOOST_CHECK(offer.price == info1.price);
    BOOST_CHECK(offer.minAmount == info1.minAmount);
    BOOST_CHECK(offer.shortInfo == info1.shortInfo);
    BOOST_CHECK(offer.countryIso == info1.countryIso);
    BOOST_CHECK(offer.currencyIso == info1.currencyIso);
    BOOST_CHECK(offer.paymentMethod == info1.paymentMethod);
    BOOST_CHECK(offer.timeCreate == info1.timeCreate);
    BOOST_CHECK(offer.timeToExpiration == info1.timeToExpiration);
    BOOST_CHECK(offer.timeModification == info1.timeModification);
    BOOST_CHECK(offer.editingVersion == info1.editingVersion);

    offer = db->getMyOfferByHash(info1.hash);

    BOOST_CHECK(offer.pubKey == info1.pubKey);
    BOOST_CHECK(offer.idTransaction == info1.idTransaction);
    BOOST_CHECK(offer.hash == info1.hash);
    BOOST_CHECK(offer.type == info1.type);
    BOOST_CHECK(offer.status == info1.status);
    BOOST_CHECK(offer.price == info1.price);
    BOOST_CHECK(offer.minAmount == info1.minAmount);
    BOOST_CHECK(offer.shortInfo == info1.shortInfo);
    BOOST_CHECK(offer.countryIso == info1.countryIso);
    BOOST_CHECK(offer.currencyIso == info1.currencyIso);
    BOOST_CHECK(offer.paymentMethod == info1.paymentMethod);
    BOOST_CHECK(offer.timeCreate == info1.timeCreate);
    BOOST_CHECK(offer.timeToExpiration == info1.timeToExpiration);
    BOOST_CHECK(offer.timeModification == info1.timeModification);
    BOOST_CHECK(offer.editingVersion == info1.editingVersion);

    auto list = db->getMyOffers();

    for (auto item : list) {
        MyOfferInfo offer = db->getMyOffer(item.idTransaction);

        BOOST_CHECK(offer.pubKey == item.pubKey);
        BOOST_CHECK(offer.idTransaction == item.idTransaction);
        BOOST_CHECK(offer.hash == item.hash);
        BOOST_CHECK(offer.price == item.price);
        BOOST_CHECK(offer.minAmount == item.minAmount);
        BOOST_CHECK(offer.shortInfo == item.shortInfo);
        BOOST_CHECK(offer.countryIso == item.countryIso);
        BOOST_CHECK(offer.currencyIso == item.currencyIso);
        BOOST_CHECK(offer.paymentMethod == item.paymentMethod);
        BOOST_CHECK(offer.timeCreate == item.timeCreate);
        BOOST_CHECK(offer.timeToExpiration == item.timeToExpiration);
        BOOST_CHECK(offer.timeModification == item.timeModification);
        BOOST_CHECK(offer.editingVersion == item.editingVersion);
    }

    db->deleteOldMyOffers();

    list = db->getMyOffers();

    BOOST_CHECK(list.empty());
}

void checkCallBack(DexDB *db)
{
    CallBackOffers cb;
    db->addCallBack(&cb);

    db->addOfferSell(OfferInfo());

    BOOST_CHECK(cb.getTypeTable() == TypeTable::OffersSell);
    BOOST_CHECK(cb.getTypeTableOperation() == TypeTableOperation::Add);
    BOOST_CHECK(cb.getStatusTableOperation() == StatusTableOperation::Ok);

    db->editOfferBuy(OfferInfo());

    BOOST_CHECK(cb.getTypeTable() == TypeTable::OffersBuy);
    BOOST_CHECK(cb.getTypeTableOperation() == TypeTableOperation::Edit);
    BOOST_CHECK(cb.getStatusTableOperation() == StatusTableOperation::Ok);
}

BOOST_FIXTURE_TEST_SUITE(dexdb_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dexdb_test)
{
    strDexDbFile = "test.db";
    remove(strDexDbFile.c_str());
    DexDB *db = DexDB::instance();

    checkCountry(db);
    checkCurrency(db);
    checkPaymentMethod(db);

    checkOffers(db);
    checkMyOffers(db);
    checkCallBack(db);

    db->freeInstance();
//    remove(dbName.c_str());
}


BOOST_AUTO_TEST_SUITE_END()
