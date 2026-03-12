#include "qf/data/recorder/csv_writer.hpp"

#include <cstring>
#include <stdexcept>
#include "qf/protocol/messages.hpp"

namespace qf::data {

namespace {

// Extract price, quantity, and order_id from the payload based on message type.
struct TickFields {
    double   price;
    uint32_t quantity;
    uint64_t order_id;
};

TickFields extract_fields(const TickRecord& record) {
    TickFields fields{0.0, 0, 0};

    switch (record.message_type) {
        case 'A': {  // AddOrder
            AddOrder msg;
            std::memcpy(&msg, record.payload, sizeof(AddOrder));
            fields.price    = price_to_double(msg.price);
            fields.quantity = msg.quantity;
            fields.order_id = msg.order_id;
            break;
        }
        case 'X': {  // CancelOrder
            CancelOrder msg;
            std::memcpy(&msg, record.payload, sizeof(CancelOrder));
            fields.order_id = msg.order_id;
            break;
        }
        case 'E': {  // ExecuteOrder
            ExecuteOrder msg;
            std::memcpy(&msg, record.payload, sizeof(ExecuteOrder));
            fields.quantity = msg.exec_quantity;
            fields.order_id = msg.order_id;
            break;
        }
        case 'R': {  // ReplaceOrder
            ReplaceOrder msg;
            std::memcpy(&msg, record.payload, sizeof(ReplaceOrder));
            fields.price    = price_to_double(msg.new_price);
            fields.quantity = msg.new_quantity;
            fields.order_id = msg.order_id;
            break;
        }
        case 'T': {  // TradeMessage
            TradeMessage msg;
            std::memcpy(&msg, record.payload, sizeof(TradeMessage));
            fields.price    = price_to_double(msg.price);
            fields.quantity = msg.quantity;
            fields.order_id = msg.buy_order_id;
            break;
        }
        default:
            break;
    }
    return fields;
}

const char* message_type_name(char type) {
    switch (type) {
        case 'A': return "ADD";
        case 'X': return "CANCEL";
        case 'E': return "EXECUTE";
        case 'R': return "REPLACE";
        case 'T': return "TRADE";
        default:  return "UNKNOWN";
    }
}

}  // anonymous namespace

CsvWriter::CsvWriter(const std::string& path) : path_(path) {
    file_ = std::fopen(path_.c_str(), "w");
    if (!file_) {
        throw std::runtime_error("CsvWriter: failed to open file: " + path_);
    }
    write_header();
}

CsvWriter::~CsvWriter() {
    if (file_) {
        close();
    }
}

void CsvWriter::write_header() {
    std::fprintf(file_, "timestamp,symbol,type,price,quantity,order_id\n");
}

void CsvWriter::write(const TickRecord& record) {
    if (!file_) {
        throw std::runtime_error("CsvWriter: file is not open");
    }

    TickFields fields = extract_fields(record);

    // Format symbol: trim trailing nulls.
    char sym[SYMBOL_LENGTH + 1];
    std::memcpy(sym, record.symbol.data, SYMBOL_LENGTH);
    sym[SYMBOL_LENGTH] = '\0';
    // Trim trailing spaces/nulls.
    for (int i = SYMBOL_LENGTH - 1; i >= 0; --i) {
        if (sym[i] == '\0' || sym[i] == ' ') {
            sym[i] = '\0';
        } else {
            break;
        }
    }

    std::fprintf(file_, "%llu,%s,%s,%.4f,%u,%llu\n",
                 static_cast<unsigned long long>(record.timestamp),
                 sym,
                 message_type_name(record.message_type),
                 fields.price,
                 static_cast<unsigned>(fields.quantity),
                 static_cast<unsigned long long>(fields.order_id));

    ++rows_written_;
}

void CsvWriter::flush() {
    if (file_) {
        std::fflush(file_);
    }
}

void CsvWriter::close() {
    if (!file_) return;
    std::fflush(file_);
    std::fclose(file_);
    file_ = nullptr;
}

uint64_t CsvWriter::convert(const std::string& binary_path,
                             const std::string& csv_path) {
    // Open the binary file for reading.
    FILE* bin = std::fopen(binary_path.c_str(), "rb");
    if (!bin) {
        throw std::runtime_error("CsvWriter::convert: cannot open binary file: " + binary_path);
    }

    // Read and validate the file header.
    FileHeader header;
    if (std::fread(&header, sizeof(FileHeader), 1, bin) != 1) {
        std::fclose(bin);
        throw std::runtime_error("CsvWriter::convert: failed to read header: " + binary_path);
    }
    if (header.magic != BINARY_FILE_MAGIC) {
        std::fclose(bin);
        throw std::runtime_error("CsvWriter::convert: invalid magic in: " + binary_path);
    }

    // Open CSV output.
    CsvWriter csv(csv_path);

    // Read records one by one.
    TickRecord record;
    uint64_t count = 0;
    while (std::fread(&record, sizeof(TickRecord), 1, bin) == 1) {
        csv.write(record);
        ++count;
    }

    csv.close();
    std::fclose(bin);

    return count;
}

}  // namespace qf::data
