
#ifndef __DEX_MANAGER_H__
#define __DEX_MANAGER_H__

#include "key.h"
#include "main.h"
#include "net.h"
#include "dex/db/dexdto.h"
#include "dex/db/dexdb.h"
#include "dex.h"
#include "dexoffer.h"
#include "unconfirmedoffers.h"


namespace dex {

class CDexManager;
extern CDexManager dexman;

static const int MIN_DEX_PROTO_VERSION = 70207;

class CDexManager
{
public:
    CDexManager();
    ~CDexManager();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void addOrEditDraftMyOffer(MyOfferInfo &myOffer);
    void prepareAndSendMyOffer(MyOfferInfo &myOffer, std::string &error);
    void sendNewOffer(const CDexOffer &offer, const CTransaction &tx);
    void sendEditedOffer(const CDexOffer &offer);
    void checkUncOffers();
    void setStatusExpiredForMyOffers();
    void deleteOldUncOffers();
    void deleteOldOffers();

    std::list<std::pair<uint256, uint32_t> > availableOfferHashAndVersion() const;
    std::list<std::pair<uint256, uint32_t> > availableOfferHashAndVersionFromBD(const DexDB::OffersPeriod &from, const uint64_t &timeMod) const;
    std::list<std::pair<uint256, uint32_t> > availableOfferHashAndVersionFromUnc() const;
    CDexOffer getOfferInfo(const uint256 &hash) const;
    UnconfirmedOffers *getUncOffers() const;
    UnconfirmedOffers *getBcstUncOffers() const;

    boost::signals2::signal<void()> startSyncDex;

private:
    DexDB *db;
    UnconfirmedOffers *uncOffers;
    UnconfirmedOffers *uncBcstOffers;

    void initDB();
    void getAndSendNewOffer(CNode* pfrom, CDataStream& vRecv);
    void getAndDelOffer(CNode* pfrom, CDataStream& vRecv);
    void getAndSendEditedOffer(CNode* pfrom, CDataStream& vRecv);

    void saveMyOffer(const MyOfferInfo &info, bool usethread = true);
};

}

void ThreadDexManager();
void ThreadDexUncManager();
void CheckDexMasternode();

#endif // __DEX_MANAGER_H__
