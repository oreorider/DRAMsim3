#include "config.h"

namespace po = boost::program_options;

Config::Config(int argc, const char* argv[])
{
    po::options_description desc{"Options"};
    desc.add_options()
        ("help, h", "Help")
        ("opcode",                 po::value<unsigned>(&opcode)->default_value(0),                     "opcode")
        ("nepochs",                po::value<int>(&nepochs)->default_value(2),                         "nepochs")
        ("batch_size",             po::value<unsigned>(&batch_size)->default_value(64),                "batch_size")
        ("embedding_table, t",     po::value<string>(&table_list)->default_value("1000000-1000000"),   "embedding table list")
        ("num_tables",             po::value<int>(&num_tables)->default_value(0),                      "num_tables")
        ("sparse_feature_size",    po::value<int>(&sparse_feature_size)->default_value(16),            "sparse_feature_size")
        ("data_type_size",         po::value<int>(&data_type_size)->default_value(4),                  "data_type_size")
        ("max_indices_per_lookup", po::value<int>(&max_indices_per_lookup)->default_value(50),         "max_indices_per_lookup")
        ("pooling_type",           po::value<int>(&pooling_type)->default_value(0),                    "pooling_type")
        ("pooling_prod_list",      po::value<string>(&pooling_prod_list)->default_value("48-122-25-50-30-70-25-25-25-40"), "num_indices_per_lookup_prod")
        ("default_interval",       po::value<int>(&default_interval)->default_value(4),                "default_interval")
        ("miss_ratio",             po::value<int>(&miss_ratio)->default_value(100),                    "miss_ratio")
        ("base_only",              po::value<bool>(&base_only)->default_value(false),                  "base_only")
        ("channel",                po::value<string>(&channel_list)->default_value("1"),               "channel")
        ("rank",                   po::value<string>(&rank_list)->default_value("0-1"),                "rank")
        ("bg_size",                po::value<int>(&bg_size)->default_value(4),                         "bg_size")
        ("ba_size",                po::value<int>(&ba_size)->default_value(4),                         "ba_size")
        ("ro_size",                po::value<int>(&ro_size)->default_value(65536),                     "ro_size")
        ("co_size",                po::value<int>(&co_size)->default_value(1024),                      "co_size")
        ("bus_width",              po::value<int>(&bus_width)->default_value(64),                      "bus_width")
        ("BL",                     po::value<int>(&BL)->default_value(8),                              "BL")
        ("address_mapping",        po::value<string>(&address_mapping)->default_value("rochrababgco"), "address_mapping")
        ("file_name, f",           po::value<string>(&file_name)->default_value("test"),               "Output trace file name")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
        cout << desc << endl;
        assert(false);
    }
    po::notify(vm);

    data_size = sparse_feature_size * data_type_size;

    channel = split(channel_list, '-');
    rank = split(rank_list, '-');
    ch_size = channel.size();
    ra_size = rank.size();

    base_out.open(file_name + "_base.trc");
    if (!base_only)
        cxlpnm_out.open(file_name + "_cxlpnm.trc");

    if (opcode == 0) {
        tables = split(table_list, '-');
        if(num_tables != 0) {
            tables.resize(num_tables, tables[0]);
        }

        accum_table_size.resize(tables.size() + 1);
        accum_table_size[0] = 0;
        for(unsigned i = 1; i < accum_table_size.size(); i++) {
            accum_table_size[i] = accum_table_size[i-1] + tables[i-1];
        }

        pooling_prod = split(pooling_prod_list, '-');

        num_indices_per_lookup.resize(tables.size()); // [table]
        indices.resize(nepochs); // [nepochs] [table] [batch] [lookup]

        total_lookup = 0;
        int p = 0;
        for (int c = 0; c < nepochs; c++) {
            indices[c].resize(tables.size());
            for (unsigned i = 0; i < tables.size(); i++) {
                if (c == 0) {
                    switch (static_cast<PoolType>(pooling_type)) {
                        case PoolType::FIXED:
                            num_indices_per_lookup[i] = max_indices_per_lookup;
                            break;
                        case PoolType::RANDOM:
                            num_indices_per_lookup[i] =
                                rand() % max_indices_per_lookup + 1;
                            break;
                        case PoolType::PROD:
                            num_indices_per_lookup[i] = pooling_prod[p];
                            p = (p + 1) % pooling_prod.size();
                            break;
                        default:
                            break;
                    }

                    assert(num_indices_per_lookup[i] <= tables[i]);

                    total_lookup += num_indices_per_lookup[i];
                }

                indices[c][i].resize(batch_size);
                for (unsigned j = 0; j < batch_size; j++) {
                    set<unsigned> unique_idx;
                    while (unique_idx.size() < num_indices_per_lookup[i]) {
                        //unsigned idx_tt = unique_idx.size();
                        unsigned idx_tt = rand() % tables[i];
                        if (unique_idx.find(idx_tt) == unique_idx.end()) {
                            indices[c][i][j].push_back(idx_tt);
                            unique_idx.insert(idx_tt);
                        }
                    }
                }
            }
        }
        // batch list
        // # of instruction / batch = total_lookup
        // Inst size = # of instruction x 8B
        // 256 KB / Inst size = batch_s
        int batch_s = INST_BUFFER_BYTE_SIZE / (total_lookup * 8);
        int tmp_batch_size = batch_size;
        while (tmp_batch_size > 0) {
            batch_list.push_back(min(batch_s, tmp_batch_size));
            tmp_batch_size -= batch_s;
        }
    }
    else{
        assert(false);
    }

    SetAddressMapping();

    uint64_t total_data_size = accum_table_size[tables.size()] * data_size;
    uint64_t memory_size = (uint32_t)1 << (addr_bits - shift_bits - ch_bits);
    if (total_data_size >= memory_size) {
        cerr << "Error: table size exceeds memory size!" << endl;
        exit(1);
    }
}

Config::~Config()
{
    base_out.close();
    cxlpnm_out.close();
}

vector<unsigned> Config::split(string str, char delimiter)
{
    vector<unsigned> internal;
    stringstream ss(str);
    string temp;

    while (getline(ss, temp, delimiter)) {
        internal.push_back(stoi(temp));
    }
    return internal;
}

void Config::SetAddressMapping()
{
    // memory addresses are byte addressable, but each request comes with
    // multiple bytes because of bus width, and burst length
    int request_size_bytes = bus_width / 8 * BL;
    shift_bits = LogBase2(request_size_bytes);
    int col_low_bits = LogBase2(BL);
    int actual_col_bits = LogBase2(co_size) - col_low_bits;

    ch_bits = LogBase2(ch_size);
    ra_bits = LogBase2(ra_size);
    bg_bits = LogBase2(bg_size);
    ba_bits = LogBase2(ba_size);
    ro_bits = LogBase2(ro_size);
    co_bits = actual_col_bits;

    // has to strictly follow the order of chan, rank, bg, bank, row, col
    std::map<std::string, int> field_widths;
    field_widths["ch"] = LogBase2(ch_size);
    field_widths["ra"] = LogBase2(ra_size);
    field_widths["bg"] = LogBase2(bg_size);
    field_widths["ba"] = LogBase2(ba_size);
    field_widths["ro"] = LogBase2(ro_size);
    field_widths["co"] = actual_col_bits;

    if (address_mapping.size() != 12) {
        std::cerr << "Unknown address mapping (6 fields each 2 chars required)"
                  << std::endl;
    }

    // // get address mapping position fields from config
    // // each field must be 2 chars
    std::vector<std::string> fields;
    for (size_t i = 0; i < address_mapping.size(); i += 2) {
        std::string token = address_mapping.substr(i, 2);
        fields.push_back(token);
    }

    std::map<std::string, int> field_pos;
    int pos = 0;
    while (!fields.empty()) {
        auto token = fields.back();
        fields.pop_back();
        if (field_widths.find(token) == field_widths.end()) {
            std::cerr << "Unrecognized field: " << token << std::endl;
        }
        field_pos[token] = pos;
        pos += field_widths[token];
    }

    ch_pos = field_pos.at("ch");
    ra_pos = field_pos.at("ra");
    bg_pos = field_pos.at("bg");
    ba_pos = field_pos.at("ba");
    ro_pos = field_pos.at("ro");
    co_pos = field_pos.at("co");

    ch_mask = (1 << field_widths.at("ch")) - 1;
    ra_mask = (1 << field_widths.at("ra")) - 1;
    bg_mask = (1 << field_widths.at("bg")) - 1;
    ba_mask = (1 << field_widths.at("ba")) - 1;
    ro_mask = (1 << field_widths.at("ro")) - 1;
    co_mask = (1 << field_widths.at("co")) - 1;

    addr_bits = 
        ch_bits + ra_bits + bg_bits + ba_bits + ro_bits + co_bits + shift_bits;
}

int LogBase2(int power_of_two)
{
    int i = 0;
    while (power_of_two > 1) {
        power_of_two /= 2;
        i++;
    }
    return i;
}
