#include "bytearray.h"

#include <string.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "endian.h"
#include "log.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

ByteArray::Node::Node(size_t s) : ptr(new char[s]), next(nullptr), size(s) {}

ByteArray::Node::Node() : ptr(nullptr), next(nullptr), size(0) {}

ByteArray::Node::~Node() {
  if (ptr) {
    delete[] ptr;
  }
}

ByteArray::ByteArray(size_t baseSize)
    : base_size_(baseSize),
      position_(0),
      capacity_(baseSize),
      size_(0),
      endian_(GUDOV_LITTLE_ENDIAN),
      root_(new Node(baseSize)),
      cur_(root_) {}

ByteArray::~ByteArray() {
  Node* tmp = root_;
  while (tmp) {
    cur_ = tmp;
    tmp  = tmp->next;
    delete cur_;
  }
}

void ByteArray::WriteFint8(int8_t value) { Write(&value, sizeof(value)); }

void ByteArray::WriteFuint8(uint8_t value) { Write(&value, sizeof(value)); }

void ByteArray::WriteFint16(int16_t value) {
  if (endian_ != GUDOV_BYTE_ORDER) {
    value = ByteSwap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFuint16(uint16_t value) {
  if (endian_ != GUDOV_BYTE_ORDER) {
    value = ByteSwap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFint32(int32_t value) {
  if (endian_ != GUDOV_BYTE_ORDER) {
    value = ByteSwap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFuint32(uint32_t value) {
  if (endian_ != GUDOV_BYTE_ORDER) {
    value = ByteSwap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFint64(int64_t value) {
  if (endian_ != GUDOV_BYTE_ORDER) {
    value = ByteSwap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFuint64(uint64_t value) {
  if (endian_ != GUDOV_BYTE_ORDER) {
    value = ByteSwap(value);
  }
  Write(&value, sizeof(value));
}

static uint32_t EncodeZigzag32(const int32_t& v) {
  if (v < 0) {
    return ((uint32_t)(-v) * 2 - 1);
  } else {
    return v * 2;
  }
}

static uint64_t EncodeZigzag64(const int64_t& v) {
  if (v < 0) {
    return ((uint64_t)(-v) * 2 - 1);
  } else {
    return v * 2;
  }
}

static int32_t DecodeZigzag32(const uint32_t& v) { return (v >> 1) ^ -(v & 1); }

static int64_t DecodeZigzag64(const uint64_t& v) { return (v >> 1) ^ -(v & 1); }

void ByteArray::WriteInt32(int32_t value) { WriteUint32(EncodeZigzag32(value)); }

void ByteArray::WriteUint32(uint32_t value) {
  uint8_t tmp[5];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  tmp[i++] = value;
  Write(tmp, i);
}

void ByteArray::WriteInt64(int64_t value) { WriteUint64(EncodeZigzag64(value)); }

void ByteArray::WriteUint64(uint64_t value) {
  uint8_t tmp[10];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  tmp[i++] = value;
  Write(tmp, i);
}

void ByteArray::WriteFloat(float value) {
  uint32_t v;
  memcpy(&v, &value, sizeof(value));
  WriteFuint32(v);
}

void ByteArray::WriteDouble(double value) {
  uint64_t v;
  memcpy(&v, &value, sizeof(value));
  WriteFuint64(v);
}

void ByteArray::WriteStringF16(const std::string& value) {
  WriteFuint16(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringF32(const std::string& value) {
  WriteFuint32(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringF64(const std::string& value) {
  WriteFuint64(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringVint(const std::string& value) {
  WriteUint64(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringWithoutLength(const std::string& value) { Write(value.c_str(), value.size()); }

int8_t ByteArray::ReadFint8() {
  int8_t v;
  Read(&v, sizeof(v));
  return v;
}

uint8_t ByteArray::ReadFuint8() {
  uint8_t v;
  Read(&v, sizeof(v));
  return v;
}

#define XX(type)                     \
  type v;                            \
  Read(&v, sizeof(v));               \
  if (endian_ == GUDOV_BYTE_ORDER) { \
    return v;                        \
  } else {                           \
    return ByteSwap(v);              \
  }

int16_t ByteArray::ReadFint16(){XX(int16_t)}

uint16_t ByteArray::ReadFuint16(){XX(uint16_t)}

int32_t ByteArray::ReadFint32(){XX(int32_t)}

uint32_t ByteArray::ReadFuint32(){XX(uint32_t)}

int64_t ByteArray::ReadFint64(){XX(int64_t)}

uint64_t ByteArray::ReadFuint64(){XX(uint64_t)}
#undef XX

int32_t ByteArray::ReadInt32() {
  return DecodeZigzag32(ReadUint32());
}

uint32_t ByteArray::ReadUint32() {
  uint32_t result = 0;
  for (int i = 0; i < 32; i += 7) {
    uint8_t b = ReadFuint8();
    if (b < 0x80) {
      result |= ((uint32_t)b) << i;
      break;
    } else {
      result |= ((uint32_t)(b & 0x7F)) << i;
    }
  }
  return result;
}

int64_t ByteArray::ReadInt64() { return DecodeZigzag64(ReadUint64()); }

uint64_t ByteArray::ReadUint64() {
  uint64_t result = 0;
  for (int i = 0; i < 64; i += 7) {
    uint8_t b = ReadFuint8();
    if (b < 0x80) {
      result |= ((uint32_t)b) << i;
      break;
    } else {
      result |= ((uint32_t)(b & 0x7F)) << i;
    }
  }
  return result;
}

float ByteArray::ReadFloat() {
  uint32_t v = ReadFuint32();
  float    value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

double ByteArray::ReadDouble() {
  uint64_t v = ReadFuint64();
  double   value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

std::string ByteArray::ReadStringF16() {
  uint16_t    len = ReadFuint16();
  std::string buff;
  buff.resize(len);
  Read(&buff[0], len);
  return buff;
}

std::string ByteArray::ReadStringF32() {
  uint32_t    len = ReadFuint32();
  std::string buff;
  buff.resize(len);
  Read(&buff[0], len);
  return buff;
}

std::string ByteArray::ReadStringF64() {
  uint64_t    len = ReadFuint64();
  std::string buff;
  buff.resize(len);
  Read(&buff[0], len);
  return buff;
}

std::string ByteArray::ReadStringVint() {
  uint64_t    len = ReadUint64();
  std::string buff;
  buff.resize(len);
  Read(&buff[0], len);
  return buff;
}

void ByteArray::Clear() {
  position_ = 0;
  size_     = 0;
  capacity_ = base_size_;
  Node* tmp = root_->next;
  while (tmp) {
    cur_ = tmp;
    tmp  = tmp->next;
    delete cur_;
  }
  cur_        = root_;
  root_->next = nullptr;
}

void ByteArray::Write(const void* buf, size_t size) {
  if (size == 0) {
    return;
  }
  AddCapacity(size);

  size_t npos = position_ % base_size_;
  size_t ncap = cur_->size - npos;
  size_t bpos = 0;

  while (size > 0) {
    if (ncap >= size) {
      memcpy(cur_->ptr + npos, (const char*)buf + bpos, size);
      if (cur_->size == (npos + size)) {
        cur_ = cur_->next;
      }
      position_ += size;
      bpos += size;
      size = 0;
    } else {
      memcpy(cur_->ptr + npos, (const char*)buf + bpos, ncap);
      position_ += ncap;
      bpos += ncap;
      size -= ncap;
      cur_ = cur_->next;
      ncap = cur_->size;
      npos = 0;
    }
  }

  if (position_ > size_) {
    // FIXIT: ReadFromFile will change size_, I don't know why
    size_ = position_;
  }
}

void ByteArray::Read(void* buf, size_t size) {
  if (size > GetReadSize()) {
    throw std::out_of_range("not enough len");
  }

  size_t npos = position_ % base_size_;
  size_t ncap = cur_->size - npos;
  size_t bpos = 0;
  while (size > 0) {
    if (ncap >= size) {
      memcpy((char*)buf + bpos, cur_->ptr + npos, size);
      if (cur_->size == (npos + size)) {
        cur_ = cur_->next;
      }
      position_ += size;
      bpos += size;
      size = 0;
    } else {
      memcpy((char*)buf + bpos, cur_->ptr + npos, ncap);
      position_ += ncap;
      bpos += ncap;
      size -= ncap;
      cur_ = cur_->next;
      ncap = cur_->size;
      npos = 0;
    }
  }
}

void ByteArray::Read(void* buf, size_t size, size_t position) const {
  if (size > (size_ - position)) {
    throw std::out_of_range("not enough len");
  }

  size_t npos = position % base_size_;
  size_t ncap = cur_->size - npos;
  size_t bpos = 0;
  Node*  cur  = cur_;
  while (size > 0) {
    if (ncap >= size) {
      memcpy((char*)buf + bpos, cur->ptr + npos, size);
      if (cur->size == (npos + size)) {
        cur = cur->next;
      }
      position += size;
      bpos += size;
      size = 0;
    } else {
      memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
      position += ncap;
      bpos += ncap;
      size -= ncap;
      cur  = cur->next;
      ncap = cur->size;
      npos = 0;
    }
  }
}

void ByteArray::SetPosition(size_t v) {
  if (v > capacity_) {
    throw std::out_of_range("set_position out of range");
  }
  position_ = v;
  if (position_ > size_) {
    size_ = position_;
  }
  cur_ = root_;
  while (v > cur_->size) {
    v -= cur_->size;
    cur_ = cur_->next;
  }
  if (v == cur_->size) {
    cur_ = cur_->next;
  }
}

bool ByteArray::WriteToFile(const std::string& name) const {
  std::ofstream ofs;
  ofs.open(name, std::ios::trunc | std::ios::binary);
  if (!ofs) {
    LOG_ERROR(g_logger) << "WriteToFile name=" << name << " error , errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }

  int64_t readSize = GetReadSize();
  int64_t pos      = position_;
  Node*   cur      = cur_;

  while (readSize > 0) {
    int     diff = pos % base_size_;
    int64_t len  = (readSize > (int64_t)base_size_ ? base_size_ : readSize) - diff;
    ofs.write(cur->ptr + diff, len);
    cur = cur->next;
    pos += len;
    readSize -= len;
  }

  return true;
}

bool ByteArray::ReadFromFile(const std::string& name) {
  std::ifstream ifs;
  ifs.open(name, std::ios::binary);
  if (!ifs) {
    LOG_ERROR(g_logger) << "readFromFile name=" << name << " error, errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }

  std::shared_ptr<char> buff(new char[base_size_], [](char* ptr) { delete[] ptr; });
  while (!ifs.eof()) {
    ifs.read(buff.get(), base_size_);
    Write(buff.get(), ifs.gcount());
  }

  return true;
}

bool ByteArray::IsLittleEndian() const { return endian_ == GUDOV_LITTLE_ENDIAN; }

void ByteArray::SetIsLittleEndian(bool val) {
  if (val) {
    endian_ = GUDOV_LITTLE_ENDIAN;
  } else {
    endian_ = GUDOV_BIG_ENDIAN;
  }
}

std::string ByteArray::ToString() const {
  std::string str;
  str.resize(GetReadSize());
  if (str.empty()) {
    return str;
  }
  Read(&str[0], str.size(), position_);
  return str;
}

std::string ByteArray::ToHexString() const {
  std::string       str = ToString();
  std::stringstream ss;

  for (size_t i = 0; i < str.size(); ++i) {
    if (i > 0 && i % 32 == 0) {
      ss << std::endl;
    }
    ss << std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)str[i] << " ";
  }

  return ss.str();
}

uint64_t ByteArray::GetReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
  len = len > GetReadSize() ? GetReadSize() : len;
  if (len == 0) {
    return 0;
  }

  uint64_t size = len;

  size_t       npos = position_ % base_size_;
  size_t       ncap = cur_->size - npos;
  struct iovec iov;
  Node*        cur = cur_;

  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len  = len;
      len          = 0;
    } else {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len  = ncap;
      len -= ncap;
      cur  = cur->next;
      ncap = cur->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return size;
}

uint64_t ByteArray::GetReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const {
  len = len > GetReadSize() ? GetReadSize() : len;
  if (len == 0) {
    return 0;
  }

  uint64_t size = len;

  size_t npos  = position % base_size_;
  size_t count = position / base_size_;
  Node*  cur   = root_;
  while (count > 0) {
    cur = cur->next;
    --count;
  }

  size_t       ncap = cur->size - npos;
  struct iovec iov;
  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len  = len;
      len          = 0;
    } else {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len  = ncap;
      len -= ncap;
      cur  = cur->next;
      ncap = cur->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return size;
}

uint64_t ByteArray::GetWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
  if (len == 0) {
    return 0;
  }
  AddCapacity(len);
  uint64_t size = len;

  size_t       npos = position_ % base_size_;
  size_t       ncap = cur_->size - npos;
  struct iovec iov;
  Node*        cur = cur_;
  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len  = len;
      len          = 0;
    } else {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len  = ncap;

      len -= ncap;
      cur  = cur->next;
      ncap = cur->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return size;
}

void ByteArray::AddCapacity(size_t size) {
  if (size == 0) {
    return;
  }
  size_t old_capacity = GetCapacity();
  if (old_capacity >= size) {
    return;
  }

  size         = size - old_capacity;
  size_t count = ceil(1.0 * size / base_size_);
  Node*  tail  = root_;
  while (tail->next) {
    tail = tail->next;
  }

  Node* first = NULL;
  for (size_t i = 0; i < count; ++i) {
    tail->next = new Node(base_size_);
    if (first == NULL) {
      first = tail->next;
    }
    tail = tail->next;
    capacity_ += base_size_;
  }

  if (old_capacity == 0) {
    cur_ = first;
  }
}

}  // namespace gudov
