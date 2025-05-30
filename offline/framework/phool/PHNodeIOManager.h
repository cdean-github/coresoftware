#ifndef PHOOL_PHNODEIOMANAGER_H
#define PHOOL_PHNODEIOMANAGER_H

//  Declaration of class PHNodeIOManager
//  Purpose: manages file IO for PHIODataNodes
//  Author: Matthias Messer

#include "PHIOManager.h"

#include "phool.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>

class PHCompositeNode;
class TBranch;
class TFile;
class TObject;
class TTree;

class PHNodeIOManager : public PHIOManager
{
 public:
  PHNodeIOManager() = default;
  PHNodeIOManager(const std::string &, const PHAccessType = PHReadOnly);
  PHNodeIOManager(const std::string &, const std::string &, const PHAccessType = PHReadOnly);
  PHNodeIOManager(const std::string &, const PHAccessType, const PHTreeType);
  ~PHNodeIOManager() override;

  // cppcheck-suppress [virtualCallInConstructor]
  void closeFile() override;
  bool write(PHCompositeNode *) override;
  void print() const override;

  bool setFile(const std::string &, const std::string &, const PHAccessType = PHReadOnly);
  PHCompositeNode *read(PHCompositeNode * = nullptr, size_t = 0);
  bool read(size_t requestedEvent);
  int readSpecific(size_t requestedEvent, const std::string &objectName);
  void selectObjectToRead(const std::string &objectName, bool readit);
  bool isSelected(const std::string &objectName);
  int isFunctional() const { return isFunctionalFlag; }
  bool SetCompressionSetting(const int level);
  uint64_t GetBytesWritten();
  uint64_t GetFileSize();
  std::map<std::string, TBranch *> *GetBranchMap();

  bool write(TObject **, const std::string &, int nodebuffersize, int nodesplitlevel);
  bool NodeExist(const std::string &nodename);

  void SplitLevel(const int split) { splitlevel = split; }
  void BufferSize(const int size) { buffersize = size; }
  int SplitLevel() const { return splitlevel; }
  int BufferSize() const { return buffersize; }
  void DisableReadCache();

private:
  int FillBranchMap();
  PHCompositeNode *reconstructNodeTree(PHCompositeNode *);
  bool readEventFromFile(size_t requestedEvent);
  static std::string getBranchClassName(TBranch *);

  TFile *file{nullptr};
  TTree *tree{nullptr};
  std::string TreeName{"T"};
  int accessMode{PHReadOnly};
  int m_CompressionSetting{505};  // ZSTD
  int isFunctionalFlag{0};        // flag to tell if that object initialized properly
  int buffersize{std::numeric_limits<int>::min()};
  int splitlevel{std::numeric_limits<int>::min()};
  std::map<std::string, TBranch *> fBranches;
  std::map<std::string, bool> objectToRead;
};

#endif
