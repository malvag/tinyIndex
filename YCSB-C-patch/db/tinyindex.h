#ifndef YCSB_C_TINYINDEX_H_
#define YCSB_C_TINYINDEX_H_

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <string>

#include "core/properties.h"
#include "core/ycsbdb.h"
#include "index.h"

#define GB(x) (x * 1024LU * 1024LU * 1024LU)

using std::cout;
using std::endl;
namespace ycsbc {
class TinyIndex : public YCSBDB {
   private:
    int db_num;
    int field_count;
    std::string db_name;
    tiny_index index;
    // std::vector<rocksdb::DB*> dbs;
    // const unsigned long long cache_size = GB(16);
    // std::shared_ptr<rocksdb::Cache> cache;

   public:
    TinyIndex(int num, utils::Properties &props)
        : db_num(num),
          field_count(
              std::stoi(props.GetProperty(CoreWorkload::FIELD_COUNT_PROPERTY,
                                          CoreWorkload::FIELD_COUNT_DEFAULT))) {
        db_name = "/home/malvag/Desktop/hy647_assignment1/store_data";
    }

    virtual ~TinyIndex() { cout << "Calling ~TinyIndex()..." << endl; }

   public:
    void Init() { index.recover((char *)db_name.c_str()); }
    void Close() { index.persist((char *)db_name.c_str()); }

    int Read(int id, const std::string &table, const std::string &key,
             const std::vector<std::string> *fields,
             std::vector<KVPair> &result) {
        // std::hash<std::string> hash_;

        char *value;
        // rocksdb::Status ret = dbs[hash_(key) %
        // db_num]->Get(rocksdb::ReadOptions(), key, &value);
        value = index.get((char *)key.c_str());
        if (value == NULL) {
            std::cout << "CANNOT FIND KEY!" << std::endl;
            exit(EXIT_FAILURE);
        }

        std::vector<std::string> tokens;
        boost::split(tokens, value, boost::is_any_of(" "));

        std::map<std::string, std::string> vmap;
        for (std::map<std::string, std::string>::size_type i = 0;
             i + 1 < tokens.size(); i += 2) {
            vmap.insert(
                std::pair<std::string, std::string>(tokens[i], tokens[i + 1]));
        }
        if (fields != NULL) {
            for (auto f : *fields) {
                std::map<std::string, std::string>::iterator it = vmap.find(f);
                if (it == vmap.end()) {
                    std::cout << "cannot find : " << f << " in DB" << std::endl;
                    exit(EXIT_FAILURE);
                }

                KVPair k = std::make_pair(f, it->second);
                result.push_back(k);
            }
        }
        free(value);

        return 0;
    }

    int Scan(int id, const std::string &table, const std::string &key, int len,
             const std::vector<std::string> *fields,
             std::vector<KVPair> &result) {
        // std::hash<std::string> hash_;
        // int items = 0;
        // bool done = false;

        // rocksdb::Iterator *it =
        //     dbs[hash_(key) % db_num]->NewIterator(rocksdb::ReadOptions());

        // it->Seek(key);
        // if (it->status().ok() != true) {
        //   std::cerr << "ERROR in status scan!" << std::endl;
        //   exit(EXIT_FAILURE);
        // }

        // while (it->Valid()) {
        //   std::string kk = it->key().ToString();
        //   std::string value = it->value().ToString();

        //   std::vector<std::string> tokens;
        //   boost::split(tokens, value, boost::is_any_of(" "));

        //   std::map<std::string, std::string> vmap;
        //   for (std::map<std::string, std::string>::size_type i = 0;
        //        i + 1 < tokens.size(); i += 2) {
        //     vmap.insert(
        //         std::pair<std::string, std::string>(tokens[i], tokens[i +
        //         1]));
        //   }

        //   for (std::map<std::string, std::string>::iterator it =
        //   vmap.begin();
        //        it != vmap.end(); ++it) {
        //     KVPair kv = std::make_pair(kk + it->first, it->second);
        //     result.push_back(kv);

        //     if (++items >= len) {
        //       done = true;
        //       break;
        //     }
        //   }

        //   if (done)
        //     break;

        //   it->Next();
        //   if (it->status().ok() != true) {
        //     std::cerr << "ERROR in status scan!" << std::endl;
        //     exit(EXIT_FAILURE);
        //   }
        // }

        // if (items == 0) {
        //   std::cerr << "ERROR zero len scan!" << std::endl;
        //   exit(EXIT_FAILURE);
        // }

        // delete it;

        return 0;
    }

    int Update(int id, const std::string &table, const std::string &key,
               std::vector<KVPair> &values) {
        if (field_count > 1) {  // this results in read-modify-write. Maybe we
                                // should use merge operator here
            //   std::hash<std::string> hash_;
            char *value;
            value = index.get((char *)key.c_str());
            if (value == NULL) {
                std::cout << "CANNOT FIND KEY!" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::vector<std::string> tokens;
            boost::split(tokens, value, boost::is_any_of(" "));

            std::map<std::string, std::string> vmap;
            for (std::map<std::string, std::string>::size_type i = 0;
                 i + 1 < tokens.size(); i += 2) {
                vmap.insert(std::pair<std::string, std::string>(tokens[i],
                                                                tokens[i + 1]));
            }

            for (auto f : values) {
                std::map<std::string, std::string>::iterator it =
                    vmap.find(f.first);
                if (it == vmap.end()) {
                    std::cout << "[2][UPDATE] Cannot find : " << f.first
                              << " in DB" << std::endl;
                    exit(EXIT_FAILURE);
                }

                it->second = f.second;
            }

            std::vector<KVPair> new_values;
            for (std::map<std::string, std::string>::iterator it = vmap.begin();
                 it != vmap.end(); ++it) {
                KVPair kv = std::make_pair(it->first, it->second);
                new_values.push_back(kv);
            }
            free(value);

            return Insert(id, table, key, new_values);
        } else {
            return Insert(id, table, key, values);
        }
    }

    int Insert(int id, const std::string &table, const std::string &key,
               std::vector<KVPair> &values) {
        // std::hash<std::string> hash_;

        std::string value;
        for (auto v : values) {
            value.append(v.first);
            value.append(1, ' ');
            value.append(v.second);
            value.append(1, ' ');
        }
        value.pop_back();

        int ret = index.put((char *)key.c_str(), (char *)value.c_str());
        if (ret != 0) {
            std::cout << "CANNOT INSERT KEY!" << std::endl;
            exit(EXIT_FAILURE);
        }

        return 0;
    }

    int Delete(int id, const std::string &table, const std::string &key) {
        std::cerr << "DELETE " << table << ' ' << key << std::endl;
        std::cerr << "Delete() not implemented [" << __FILE__ << ":" << __func__
                  << "():" << __LINE__ << "]" << std::endl;
        exit(EXIT_FAILURE);
    }
};
}  // namespace ycsbc

#endif  // YCSB_C_TINYINDEX_H_
