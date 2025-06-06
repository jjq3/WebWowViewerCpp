//
// Created by Deamon on 11/22/2017.
//

#ifndef WEBWOWVIEWERCPP_CASCREQUESTPROCESSOR_H
#define WEBWOWVIEWERCPP_CASCREQUESTPROCESSOR_H

#include "../../wowViewerLib/src/persistence/RequestProcessor.h"
#include "../database/buildInfoParser/buildDefinition.h"
#include <iostream>

const std::string CASC_KEYS_FILE = "KnownCascKeys.txt";

class CascRequestProcessor : public RequestProcessor {
public:
    CascRequestProcessor(const std::string &path, const BuildDefinition &buildDef);
    ~CascRequestProcessor() override;

    void updateKeys();
private:
    std::string m_cascDir = "";

    void* m_storage = nullptr;
    void* m_storageOnline = nullptr;
protected:
    void processFileRequest(const std::string &fileName, CacheHolderType holderType, const std::weak_ptr<PersistentFile> &s_file) override;
    void iterateFilesInternal(
        std::function<bool (int fileDataId, const std::string &fileName)> &process,
        std::function<void (int fileDataId, const HFileContent &fileData)> &callback) override;
private:
    HFileContent tryGetFile(void *cascStorage, void *fileNameToPass, uint32_t openFlags);
    HFileContent tryGetFileFromOverrides(int fileDataId);

    HFileContent readFileContent(const std::string &fileName, uint32_t fileDataId);
};


#endif //WEBWOWVIEWERCPP_CASCREQUESTPROCESSOR_H
