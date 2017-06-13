/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <vector>

#include "PatriciaTreeSet.h"

using pt_set = PatriciaTreeSet<uint32_t>;

class PatriciaTreeSetTest : public ::testing::Test {
 protected:
  PatriciaTreeSetTest()
      : m_rd_device(),
        m_generator(m_rd_device()),
        m_size_dist(0, 50),
        m_elem_dist(0, std::numeric_limits<uint32_t>::max()) {}

  pt_set generate_random_set() {
    pt_set s;
    size_t size = m_size_dist(m_generator);
    for (size_t i = 0; i < size; ++i) {
      s.insert(m_elem_dist(m_generator));
    }
    return s;
  }

  std::random_device m_rd_device;
  std::mt19937 m_generator;
  std::uniform_int_distribution<uint32_t> m_size_dist;
  std::uniform_int_distribution<uint32_t> m_elem_dist;
};

namespace {

std::vector<uint32_t> get_union(const std::vector<uint32_t>& a,
                                const std::vector<uint32_t>& b) {
  std::set<uint32_t> u;
  for (uint32_t x : a) {
    u.insert(x);
  }
  for (uint32_t x : b) {
    u.insert(x);
  }
  return std::vector<uint32_t>(u.begin(), u.end());
}

std::vector<uint32_t> get_intersection(const std::vector<uint32_t>& a,
                                       const std::vector<uint32_t>& b) {
  std::set<uint32_t> i, sb;
  for (uint32_t x : b) {
    sb.insert(x);
  }
  for (uint32_t x : a) {
    if (sb.count(x)) {
      i.insert(x);
    }
  }
  return std::vector<uint32_t>(i.begin(), i.end());
}

} // namespace

TEST_F(PatriciaTreeSetTest, basicOperations) {
  const uint32_t bigint = std::numeric_limits<uint32_t>::max();
  pt_set s1;
  pt_set empty_set;
  std::vector<uint32_t> elements1 = {0, 1, 2, 3, 4, 1023, bigint};

  for (uint32_t x : elements1) {
    s1.insert(x);
  }
  EXPECT_EQ(7, s1.size());
  {
    std::vector<uint32_t> v(s1.begin(), s1.end());
    EXPECT_THAT(v, ::testing::UnorderedElementsAreArray(elements1));
  }

  for (uint32_t x : elements1) {
    EXPECT_TRUE(s1.contains(x));
    EXPECT_FALSE(empty_set.contains(x));
  }
  EXPECT_FALSE(s1.contains(17));
  EXPECT_FALSE(s1.contains(1000000));

  pt_set s2 = s1;
  std::vector<uint32_t> elements2 = {0, 2, 3, 1023};
  s2.remove(1).remove(4).remove(bigint);
  {
    // We copy s1 into s2 and then we remove some elements from s2. Since the
    // underlying Patricia trees are shared after the copy, we want to make sure
    // that s1 hasn't been modified.
    std::vector<uint32_t> v(s1.begin(), s1.end());
    EXPECT_THAT(v, ::testing::UnorderedElementsAreArray(elements1));
  }
  {
    std::vector<uint32_t> v(s2.begin(), s2.end());
    EXPECT_THAT(v, ::testing::UnorderedElementsAreArray(elements2));
    std::ostringstream out;
    out << s2;
    EXPECT_EQ("{0, 2, 3, 1023}", out.str());
  }

  EXPECT_TRUE(empty_set.is_subset_of(s1));
  EXPECT_FALSE(s1.is_subset_of(empty_set));
  EXPECT_TRUE(s2.is_subset_of(s1));
  EXPECT_FALSE(s1.is_subset_of(s2));
  EXPECT_TRUE(s1.equals(s1));
  EXPECT_TRUE(empty_set.equals(empty_set));
  EXPECT_FALSE(empty_set.equals(s1));

  std::vector<uint32_t> elements3 = {2, 1023, 4096, 13001, bigint};
  pt_set s3(elements3.begin(), elements3.end());
  pt_set u13 = s1;
  u13.union_with(s3);
  EXPECT_TRUE(s1.is_subset_of(u13));
  EXPECT_TRUE(s3.is_subset_of(u13));
  EXPECT_FALSE(u13.is_subset_of(s1));
  EXPECT_FALSE(u13.is_subset_of(s3));
  {
    std::vector<uint32_t> union13 = get_union(elements1, elements3);
    std::vector<uint32_t> v(u13.begin(), u13.end());
    EXPECT_THAT(v, ::testing::UnorderedElementsAreArray(union13));
  }
  EXPECT_TRUE(s1.get_union_with(empty_set).equals(s1));
  EXPECT_TRUE(s1.get_union_with(s1).equals(s1));

  pt_set i13 = s1;
  i13.intersection_with(s3);
  EXPECT_TRUE(i13.is_subset_of(s1));
  EXPECT_TRUE(i13.is_subset_of(s3));
  EXPECT_FALSE(s1.is_subset_of(i13));
  EXPECT_FALSE(s3.is_subset_of(i13));
  {
    std::vector<uint32_t> intersection13 =
        get_intersection(elements1, elements3);
    std::vector<uint32_t> v(i13.begin(), i13.end());
    EXPECT_THAT(v, ::testing::UnorderedElementsAreArray(intersection13));
  }
  EXPECT_TRUE(s1.get_intersection_with(empty_set).is_empty());
  EXPECT_TRUE(empty_set.get_intersection_with(s1).is_empty());
  EXPECT_TRUE(s1.get_intersection_with(s1).equals(s1));

  EXPECT_EQ(elements3.size(), s3.size());
  s3.clear();
  EXPECT_EQ(0, s3.size());
}

TEST_F(PatriciaTreeSetTest, robustness) {
  for (size_t k = 0; k < 10; ++k) {
    pt_set s1 = this->generate_random_set();
    pt_set s2 = this->generate_random_set();
    auto elems1 = std::vector<uint32_t>(s1.begin(), s1.end());
    auto elems2 = std::vector<uint32_t>(s2.begin(), s2.end());
    std::vector<uint32_t> ref_u12 = get_union(elems1, elems2);
    std::vector<uint32_t> ref_i12 = get_intersection(elems1, elems2);
    pt_set u12 = s1.get_union_with(s2);
    pt_set i12 = s1.get_intersection_with(s2);
    std::vector<uint32_t> v_u12(u12.begin(), u12.end());
    std::vector<uint32_t> v_i12(i12.begin(), i12.end());
    EXPECT_THAT(v_u12, ::testing::UnorderedElementsAreArray(ref_u12))
        << "s1 = " << s1 << ", s2 = " << s2;
    EXPECT_THAT(v_i12, ::testing::UnorderedElementsAreArray(ref_i12))
        << "s1 = " << s1 << ", s2 = " << s2;
    EXPECT_TRUE(s1.is_subset_of(u12));
    EXPECT_TRUE(s2.is_subset_of(u12));
    EXPECT_TRUE(i12.is_subset_of(s1));
    EXPECT_TRUE(i12.is_subset_of(s2));
  }
}

TEST_F(PatriciaTreeSetTest, whiteBox) {
  // The algorithms are designed in such a way that Patricia trees that are left
  // unchanged by an operation are not reconstructed (i.e., the result of an
  // operation shares structure with the operands whenever possible). This is
  // what we check here.
  for (size_t k = 0; k < 10; ++k) {
    pt_set s = this->generate_random_set();
    pt_set u = s.get_union_with(s);
    pt_set i = s.get_intersection_with(s);
    EXPECT_EQ(s.get_patricia_tree(), u.get_patricia_tree());
    EXPECT_EQ(s.get_patricia_tree(), i.get_patricia_tree());
    {
      s.insert(17);
      auto tree = s.get_patricia_tree();
      s.insert(17);
      EXPECT_EQ(tree, s.get_patricia_tree());
    }
    {
      s.remove(157);
      auto tree = s.get_patricia_tree();
      s.remove(157);
      EXPECT_EQ(tree, s.get_patricia_tree());
    }
    pt_set t = this->generate_random_set();
    pt_set ust = s.get_union_with(t);
    pt_set ist = s.get_intersection_with(t);
    auto ust_tree = ust.get_patricia_tree();
    auto ist_tree = ist.get_patricia_tree();
    ust.union_with(t);
    ist.intersection_with(t);
    EXPECT_EQ(ust.get_patricia_tree(), ust_tree);
    EXPECT_EQ(ist.get_patricia_tree(), ist_tree);
  }
}

TEST_F(PatriciaTreeSetTest, setsOfPointers) {
  using string_set = PatriciaTreeSet<std::string*>;
  std::string a = "a";
  std::string b = "b";
  std::string c = "c";
  std::string d = "d";

  string_set s;
  s.insert(&a).insert(&b).insert(&c).insert(&d);
  {
    std::vector<std::string> v;
    for (std::string* p : s) {
      v.push_back(*p);
    }
    EXPECT_THAT(v, ::testing::UnorderedElementsAre("a", "b", "c", "d"));
  }

  s.remove(&a).remove(&d);
  {
    std::vector<std::string> v;
    for (std::string* p : s) {
      v.push_back(*p);
    }
    EXPECT_THAT(v, ::testing::UnorderedElementsAre("b", "c"));
  }
}
