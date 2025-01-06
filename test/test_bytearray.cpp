#include <gtest/gtest.h>

#include "gudov/bytearray.h"
#include "gudov/gudov.h"

using namespace gudov;

TEST(ByteArrayTest, WriteReadInt8) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  int8_t write_value = -100;
  ba->WriteFint8(write_value);
  ba->SetPosition(0);

  int8_t read_value = ba->ReadFint8();
  ASSERT_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadUInt8) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  uint8_t write_value = 200;
  ba->WriteFuint8(write_value);
  ba->SetPosition(0);

  uint8_t read_value = ba->ReadFuint8();
  ASSERT_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadInt32) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  int32_t write_value = -12345678;
  ba->WriteFint32(write_value);
  ba->SetPosition(0);

  int32_t read_value = ba->ReadFint32();
  ASSERT_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadUInt32) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  uint32_t write_value = 1234567890;
  ba->WriteFuint32(write_value);
  ba->SetPosition(0);

  uint32_t read_value = ba->ReadFuint32();
  ASSERT_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadFloat) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  float write_value = 123.456f;
  ba->WriteFloat(write_value);
  ba->SetPosition(0);

  float read_value = ba->ReadFloat();
  ASSERT_FLOAT_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadDouble) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  double write_value = 123456.789;
  ba->WriteDouble(write_value);
  ba->SetPosition(0);

  double read_value = ba->ReadDouble();
  ASSERT_DOUBLE_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadString) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  std::string write_value = "Hello, ByteArray!";
  ba->WriteStringF16(write_value);
  ba->SetPosition(0);

  std::string read_value = ba->ReadStringF16();
  ASSERT_EQ(write_value, read_value);
}

TEST(ByteArrayTest, WriteReadByteArray) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  const char* data = "Test ByteArray";
  size_t      size = strlen(data) + 1;  // Include null terminator
  ba->Write(data, size);

  ba->SetPosition(0);
  char buffer[50];
  ba->Read(buffer, size);

  ASSERT_STREQ(data, buffer);
}

TEST(ByteArrayTest, WriteToFileAndReadFromFile) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  // Write some data
  int32_t write_value = 12345678;
  ba->WriteFint32(write_value);

  std::string filename = "test_bytearray.dat";
  ASSERT_TRUE(ba->WriteToFile(filename));

  // Create a new ByteArray and read from the file
  ByteArray::ptr ba2 = std::make_shared<ByteArray>();
  ASSERT_TRUE(ba2->ReadFromFile(filename));

  EXPECT_EQ(ba2->GetSize(), ba->GetSize());

  // Verify the content
  ba2->SetPosition(0);
  int32_t read_value = ba2->ReadFint32();
  ASSERT_EQ(write_value, read_value);

  // Cleanup the test file
  std::remove(filename.c_str());
}

TEST(ByteArrayTest, DynamicResize) {
  ByteArray::ptr ba = std::make_shared<ByteArray>(10);

  // Write more data than the initial base size
  for (int i = 0; i < 100; ++i) {
    ba->WriteFint32(i);
  }

  // Verify the data
  ba->SetPosition(0);
  for (int i = 0; i < 100; ++i) {
    int32_t value = ba->ReadFint32();
    ASSERT_EQ(value, i);
  }
}

TEST(ByteArrayTest, EndianCheck) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  // Default is system's endian
  ASSERT_EQ(ba->IsLittleEndian(), (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__));

  // Switch endian
  ba->SetIsLittleEndian(false);
  ASSERT_FALSE(ba->IsLittleEndian());

  ba->SetIsLittleEndian(true);
  ASSERT_TRUE(ba->IsLittleEndian());
}

TEST(ByteArrayTest, Clear) {
  ByteArray::ptr ba = std::make_shared<ByteArray>();

  ba->WriteFint32(12345678);
  ASSERT_EQ(ba->GetSize(), sizeof(int32_t));

  ba->Clear();
  ASSERT_EQ(ba->GetSize(), 0);
  ASSERT_EQ(ba->GetPosition(), 0);
}