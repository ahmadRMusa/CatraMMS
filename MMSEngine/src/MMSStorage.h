

#ifndef MMSStorage_h
#define MMSStorage_h

#include <mutex>
#include <vector>
#include "spdlog/spdlog.h"
#include "catralibraries/FileIO.h"
#include "Customer.h"
#include "MMSEngineDBFacade.h"


class MMSStorage
{
public:
    enum class RepositoryType
    {
        MMSREP_REPOSITORYTYPE_MMSCUSTOMER	= 0,
        MMSREP_REPOSITORYTYPE_DOWNLOAD,
        MMSREP_REPOSITORYTYPE_STREAMING,
        MMSREP_REPOSITORYTYPE_STAGING,
        MMSREP_REPOSITORYTYPE_DONE,
        MMSREP_REPOSITORYTYPE_ERRORS,
        MMSREP_REPOSITORYTYPE_INGESTION,

        MMSREP_REPOSITORYTYPE_NUMBER
    };

public:
    MMSStorage (
            Json::Value configuration,
            shared_ptr<spdlog::logger> logger);

    ~MMSStorage (void);

    string getCustomerIngestionRepository(shared_ptr<Customer> customer);
    
    //    const char *getIPhoneAliasForLive (void);

    string getMMSRootRepository (void);

    string getStreamingRootRepository (void);

    string getDownloadRootRepository (void);

    string getIngestionRootRepository (void);
    
    string getStagingRootRepository (void);

    string getErrorRootRepository (void);

    string getDoneRootRepository (void);

    void refreshPartitionsFreeSizes (void);

    void moveContentInRepository (
        string filePathName,
        RepositoryType rtRepositoryType,
        string customerDirectoryName,
        bool addDateTimeToFileName);

    void copyFileInRepository (
	string filePathName,
	RepositoryType rtRepositoryType,
	string customerDirectoryName,
	bool addDateTimeToFileName);

    string moveAssetInMMSRepository (
        string sourceAssetPathName,
        string customerDirectoryName,
        string destinationFileName,
        string relativePath,

        bool isPartitionIndexToBeCalculated,
        unsigned long *pulMMSPartitionIndexUsed,	// OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        bool deliveryRepositoriesToo,
        Customer::TerritoriesHashMap& phmTerritories
    );

    string getMMSAssetPathName (
	unsigned long ulPartitionNumber,
	string customerDirectoryName,
	string relativePath,		// using '/'
	string fileName);

    string getDownloadLinkPathName (
	unsigned long ulPartitionNumber,
	string customerDirectoryName,
	string territoryName,
	string relativePath,
	string fileName,
	bool downloadRepositoryToo);

    string getStreamingLinkPathName (
	unsigned long ulPartitionNumber,	// IN
	string customerDirectoryName,	// IN
	string territoryName,	// IN
	string relativePath,	// IN
	string fileName);	// IN

    // bRemoveLinuxPathIfExist: often this method is called 
    // to get the path where the encoder put his output
    // (file or directory). In this case it is good
    // to clean/remove that path if already existing in order
    // to give to the encoder a clean place where to write
    string getStagingAssetPathName (
	string customerDirectoryName,
	string relativePath,
	string fileName,                // may be empty ("")
	long long llMediaItemKey,       // used only if fileName is ""
	long long llPhysicalPathKey,    // used only if fileName is ""
	bool removeLinuxPathIfExist);

    string getEncodingProfilePathName (
	long long llEncodingProfileKey,
	string profileFileNameExtension);

    string getFFMPEGEncodingProfilePathName(
        MMSEngineDBFacade::ContentType contentType,
        long long llEncodingProfileKey);

    unsigned long getCustomerStorageUsage (
	string customerDirectoryName);

private:
    shared_ptr<spdlog::logger>  _logger;

    string                      _hostName;

    string                      _storage;
    string                      _mmsRootRepository;
    string                      _downloadRootRepository;
    string                      _streamingRootRepository;
    string                      _stagingRootRepository;
    string                      _doneRootRepository;
    string                      _errorRootRepository;
    string                      _ingestionRootRepository;
    string                      _profilesRootRepository;

    unsigned long long          _freeSpaceToLeaveInEachPartitionInMB;

    recursive_mutex                 _mtMMSPartitions;
    vector<unsigned long long>      _mmsPartitionsFreeSizeInMB;
    unsigned long                   _ulCurrentMMSPartitionIndex;

    
    void contentInRepository (
	unsigned long ulIsCopyOrMove,
	string contentPathName,
	RepositoryType rtRepositoryType,
	string customerDirectoryName,
	bool addDateTimeToFileName);

    string getRepository(RepositoryType rtRepositoryType);

    string creatingDirsUsingTerritories (
	unsigned long ulCurrentMMSPartitionIndex,
	string relativePath,
	string customerDirectoryName,
	bool deliveryRepositoriesToo,
	Customer::TerritoriesHashMap& phmTerritories);


} ;

#endif
