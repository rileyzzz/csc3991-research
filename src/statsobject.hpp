#ifndef _STATSOBJECT_HPP
#define _STATSOBJECT_HPP
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <fstream>

class StatsObject
{
public:
  StatsObject(const std::string& filename, const std::vector<std::string>& columns)
    : filename(filename), columns(columns)
  {
    AddColumnsHeader();
  }

  void AddData(const std::vector<std::string>& data)
  {
    assert(data.size() == columns.size());
    for (int i = 0; i < data.size(); i++)
    {
      if (i != 0)
        rows << ",";

      rows << data[i];
    }
    rows << "\n";
    bHasData = true;
  }

  void Save()
  {
    std::ofstream ofs(filename);
    ofs << rows.rdbuf();
    ofs.close();
  }

  void Clear()
  {
    bHasData = false;
    rows = std::stringstream();
    AddColumnsHeader();
  }

  inline bool HasData() const { return bHasData; }

protected:

  void AddColumnsHeader()
  {
    for (int i = 0; i < columns.size(); i++)
    {
      if (i != 0)
        rows << ",";

      rows << columns[i];
    }
    rows << "\n";
  }

  std::string filename;
  std::vector<std::string> columns;
  std::stringstream rows;
  bool bHasData = false;
};

#endif // _STATSOBJECT_HPP