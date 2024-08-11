#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <cstdlib>
#include <ctime>
#include <cmath>

using namespace std;

struct CacheConfig 
{
    int size;
    int block_size;
    int associativity;
    string replacement_policy;
    string writeback_policy;
};

struct Access 
{
    char mode;
    unsigned int address;
};

struct CacheBlock 
{
    bool valid;
    bool dirty;
    int tag;
};

void readCacheConfig(const string &filename, CacheConfig &config) 
{
    ifstream file(filename);
    if (file.is_open()) 
    {
        file >> config.size >> config.block_size >> config.associativity >> config.replacement_policy >> config.writeback_policy;
        file.close();
    } 
    else 
    {
        cerr << "Unable to open cache configuration file." << endl;
        exit(EXIT_FAILURE);
    }
}

void readAccessSequence(const string &filename, vector<Access> &sequence) 
{
    ifstream file(filename);
    if (file.is_open()) 
    {
        string line;
        while (getline(file, line)) 
        {
            istringstream iss(line);
            char mode;
            string dummy; // To read the colon after mode
            unsigned int address;

            iss >> mode >> dummy >> hex >> address;
            if (iss.fail()) 
            {
                cerr << "Error reading access sequence: " << line << endl;
                continue;
            }

            sequence.push_back({mode, address});
        }
        file.close();
    } 
    else 
    {
        cerr << "Unable to open access sequence file." << endl;
        exit(EXIT_FAILURE);
    }
}

void directMapped(CacheConfig* cacheConfig, vector<Access> accessSequence)
{
    int words_per_block = cacheConfig->block_size/32;
    int offset_bits = log2(words_per_block);
    int num_of_blocks = cacheConfig->size/cacheConfig->block_size;
    int index_bits = log2(num_of_blocks);

    // Use a vector of CacheBlocks instead of unsigned int array
    vector<CacheBlock> cache(num_of_blocks, {false, false, INT32_MAX}); 

    Access word;
    int index;

    for(int i=0; i<accessSequence.size(); i++)
    {
        word = accessSequence[i];
        index = ((word.address)>>(offset_bits))%(num_of_blocks);

        // Check if the valid bit is set
        if (cache[index].valid && cache[index].tag == ((word.address)>>(offset_bits+index_bits)))
        {
            cout << "Address: 0x" << hex << word.address << ", Index: 0x" << index << ", Hit, Tag: 0x" 
                << hex << ((word.address)>>(offset_bits+index_bits)) << endl;
        }
        else
        {
            cout << "Address: 0x" << hex << word.address << ", Index: 0x" << index << ", Miss, Tag: 0x" 
                << hex << ((word.address)>>(offset_bits+index_bits)) << endl;

            // Update cache block with new tag and set the valid bit
            cache[index].tag = ((word.address)>>(offset_bits+index_bits));
            cache[index].valid = true;
        }
        if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
            cache[index].dirty = true;
    }
}

void fullyAssociative(CacheConfig* cacheConfig, vector<Access> accessSequence)
{
    int words_per_block = cacheConfig->block_size/32;
    int offset_bits = log2(words_per_block);
    int num_of_blocks = cacheConfig->size/cacheConfig->block_size;
    string policy = cacheConfig->replacement_policy;

    // Use a vector of CacheBlocks instead of unsigned int array
    vector<CacheBlock> cache(num_of_blocks, {false, false, INT32_MAX});

    Access word;

    if (policy == "FIFO")
    {
        queue<int> indexNo;

        for (int i = 0; i < accessSequence.size(); i++)
        {
            word = accessSequence[i];
            int tag = word.address >> offset_bits;
            int check = 0;

            // Check if the tag is already in the cache
            for (int j = 0; j < num_of_blocks; j++)
            {
                if (cache[j].valid && tag == cache[j].tag)
                {
                    cout << "Address: 0x" << hex << word.address << ", Index: 0x" << j << ", Hit, Tag: 0x" << 
                        hex << tag << endl;

                    check = 1;
                    break;
                }

                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[j].dirty = true;
            }

            // If the tag is not in the cache
            if (check == 0)
            {
                int j;
                if (indexNo.size() < num_of_blocks)// Cache is not full yet
                    j = indexNo.size();
                else
                {
                    // Cache is full, evict the oldest block
                    j = indexNo.front();
                    indexNo.pop();
                }

                cout << "Address: 0x" << hex << word.address << ", Index: 0x" << j << ", Miss, Tag: 0x" << 
                    hex << tag << endl;

                cache[j].tag = tag;
                cache[j].valid = true;
                indexNo.push(j);
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[j].dirty = true;
            }
        }
    }
    else if(policy == "LRU")
    {
        int counter[num_of_blocks] = {0}; // Counter for LRU policy

        for (int i = 0; i < accessSequence.size(); i++)
        {
            word = accessSequence[i];
            int tag = word.address >> offset_bits;
            int check = 0;
            int j;

            // Check if the tag is already in the cache
            for (j = 0; j < num_of_blocks; j++)
            {
                if (cache[j].valid && tag == cache[j].tag)
                {
                    cout << "Address: 0x" << hex << word.address << ", Index: 0x" << j << ", Hit, Tag: 0x" << 
                        hex << tag << endl;

                    // Update counters for LRU
                    for (int k = 0; k < num_of_blocks; k++)
                    {
                        if (k != j)
                            counter[k]++;
                        else
                            counter[k] = 0; // Reset the counter for the accessed line
                    }

                    check = 1;
                    break;
                }
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[j].dirty = true;
            }

            // If the tag is not in the cache
            if (check == 0)
            {
                // Find the least recently used block
                int minIndex = 0;
                for (int k = 1; k < num_of_blocks; k++)
                {
                    if (counter[k] > counter[minIndex])
                        minIndex = k;
                }

                j = minIndex;

                cout << "Address: 0x" << hex << word.address << ", Index: 0x" << j << ", Miss, Tag: 0x" << 
                    hex << tag << endl;


                cache[j].tag = tag;
                cache[j].valid = true;

                // Update counters for LRU
                for (int k = 0; k < num_of_blocks; k++)
                    counter[k]++;
                counter[j] = 0; // Reset the counter for the newly added line
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[j].dirty = true;
            }
        }
    }
    else if(policy == "RANDOM")
    {
        srand(static_cast<unsigned int>(time(nullptr))); // Seed for random numbers

        for (int i = 0; i < accessSequence.size(); i++)
        {
            word = accessSequence[i];
            int tag = word.address >> offset_bits;
            int check = 0;
            int j;

            // Check if the tag is already in the cache
            for (j = 0; j < num_of_blocks; j++)
            {
                if (cache[j].valid && tag == cache[j].tag)
                {
                    cout << "Address: 0x" << hex << word.address << ", Index: 0x" << j << ", Hit, Tag: 0x" << 
                        hex << tag << endl;

                    check = 1;
                    break;
                }
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[j].dirty = true;
            }

            // If the tag is not in the cache
            if (check == 0)
            {
                // Randomly choose a block to replace
                j = rand() % num_of_blocks;

                cout << "Address: 0x" << hex << word.address << ", Index: 0x" << j << ", Miss, Tag: 0x" << 
                    hex << tag << endl;


                cache[j].tag = tag;
                cache[j].valid = true;
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[j].dirty = true;
            }
        }
    }
    else
        cout << "Invalid replacement policy" << endl;
}

void setAssociative(CacheConfig* cacheConfig, vector<Access> accessSequence) 
{
    int ways = cacheConfig->associativity;
    int words_per_block = cacheConfig->block_size / 32;
    int offset_bits = log2(words_per_block);
    int num_of_blocks = cacheConfig->size / cacheConfig->block_size;
    int num_of_sets = num_of_blocks / ways;
    int set_bits = log2(num_of_sets);
    string policy = cacheConfig->replacement_policy;

    // Create a 2D array to represent the cache [sets][ways]
    vector<vector<CacheBlock>> cache(num_of_sets, vector<CacheBlock>(ways, {false, false, INT32_MAX}));

    Access word;

    if (policy == "FIFO") 
    {
        queue<int> indexNo;

        for (int i = 0; i < accessSequence.size(); i++) 
        {
            word = accessSequence[i];
            int set_index = ((word.address) >> offset_bits) % num_of_sets;
            int tag = word.address >> (offset_bits + set_bits);
            int check = 0;

            // Check if the tag is already in the cache
            for (int j = 0; j < ways; j++) 
            {
                if (cache[set_index][j].valid && tag == cache[set_index][j].tag) 
                {
                    cout << "Address: 0x" << hex << word.address << ", Set: " << set_index << ", Way: " << j
                         << ", Hit, Tag: 0x" << hex << tag << endl;
                    check = 1;
                    break;
                }
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[set_index][j].dirty = true;
            }

            // If the tag is not in the cache
            if (check == 0) 
            {
                int j;
                if (indexNo.size() < ways)// Cache is not full yet
                    j = indexNo.size();
                else 
                {
                    // Cache is full, evict the oldest block
                    j = indexNo.front();
                    indexNo.pop();
                }

                cout << "Address: 0x" << hex << word.address << ", Set: " << set_index << ", Way: " << j
                     << ", Miss, Tag: 0x" << hex << tag << endl;

                cache[set_index][j].tag = tag;
                cache[set_index][j].valid = true;
                indexNo.push(j);
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[set_index][j].dirty = true;
            }
        }
    } 
    else if (policy == "LRU") 
    {
        vector<vector<int>> counter(num_of_sets, vector<int>(ways, 0)); // Counter for LRU policy

        for (int i = 0; i < accessSequence.size(); i++)
        {
            word = accessSequence[i];
            int set_index = ((word.address) >> offset_bits) % num_of_sets;
            int tag = word.address >> (offset_bits + set_bits);
            int check = 0;
            int j;

            // Check if the tag is already in the cache
            for (j = 0; j < ways; j++)
            {
                if (cache[set_index][j].valid && tag == cache[set_index][j].tag)
                {
                    cout << "Address: 0x" << hex << word.address << ", Set: " << set_index << ", Way: " << j
                        << ", Hit, Tag: 0x" << hex << tag << endl;

                    // Update counters for LRU
                    for (int k = 0; k < ways; k++)
                    {
                        if (k != j)
                            counter[set_index][k]++;
                        else
                            counter[set_index][k] = 0; // Reset the counter for the accessed line
                    }

                    check = 1;
                    break;
                }
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[set_index][j].dirty = true;
            }

            // If the tag is not in the cache
            if (check == 0)
            {
                // Find the least recently used block within the set
                int minIndex = 0;
                for (int k = 1; k < ways; k++)
                {
                    if (counter[set_index][k] > counter[set_index][minIndex])
                        minIndex = k;
                }

                j = minIndex;

                cout << "Address: 0x" << hex << word.address << ", Set: " << set_index << ", Way: " << j
                    << ", Miss, Tag: 0x" << hex << tag << endl;

                cache[set_index][j].tag = tag;
                cache[set_index][j].valid = true;

                // Update counters for LRU
                for (int k = 0; k < ways; k++)
                    counter[set_index][k]++;
                counter[set_index][j] = 0; // Reset the counter for the newly added line
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[set_index][j].dirty = true;
            }
        }
    } 
    else if (policy == "RANDOM") 
    {
        srand(static_cast<unsigned int>(time(nullptr))); // Seed for random numbers

        for (int i = 0; i < accessSequence.size(); i++) 
        {
            word = accessSequence[i];
            int set_index = ((word.address) >> offset_bits) % num_of_sets;
            int tag = word.address >> (offset_bits + set_bits);
            int check = 0;
            int j;

            // Check if the tag is already in the cache
            for (j = 0; j < ways; j++) 
            {
                if (cache[set_index][j].valid && tag == cache[set_index][j].tag) 
                {
                    cout << "Address: 0x" << hex << word.address << ", Set: " << set_index << ", Way: " << j
                         << ", Hit, Tag: 0x" << hex << tag << endl;

                    check = 1;
                    break;
                }
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[set_index][j].dirty = true;
            }

            // If the tag is not in the cache
            if (check == 0) 
            {
                // Randomly choose a block to replace
                j = rand() % ways;

                cout << "Address: 0x" << hex << word.address << ", Set: " << set_index << ", Way: " << j
                     << ", Miss, Tag: 0x" << hex << tag << endl;

                cache[set_index][j].tag = tag;
                cache[set_index][j].valid = true;
                if(cacheConfig->writeback_policy == "WB" && word.mode == 'W')
                    cache[set_index][j].dirty = true;
            }
        }
    } 
    else
        cout << "Invalid replacement policy" << endl;
}


int main() 
{
    CacheConfig cacheConfig;
    readCacheConfig("cache.config", cacheConfig);
    int c = cacheConfig.associativity;

    vector<Access> accessSequence;
    readAccessSequence("cache.access", accessSequence);

    switch(c)
    {
        case 0:
        //Fully Associative
        fullyAssociative(&cacheConfig, accessSequence);
        break;

        case 1:
        //Direct Mapping
        directMapped(&cacheConfig, accessSequence);
        break;

        case 2:
        //2-way
        setAssociative(&cacheConfig, accessSequence);
        break;

        case 4:
        //4-way
        setAssociative(&cacheConfig, accessSequence);
        break;

        case 8:
        //8-way
        setAssociative(&cacheConfig, accessSequence);
        break;

        case 16:
        //16-way
        setAssociative(&cacheConfig, accessSequence);
        break;

        default:
        cout << "Invalid Associativity" << endl;
    }
    return 0;
}
