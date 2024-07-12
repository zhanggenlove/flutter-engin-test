// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Included first as it collides with the X11 headers.
#include "gtest/gtest.h"

#include "flutter/shell/platform/linux/fl_view_accessible.h"
#include "flutter/shell/platform/linux/public/flutter_linux/fl_engine.h"
#include "flutter/shell/platform/linux/testing/fl_test.h"
#include "flutter/shell/platform/linux/testing/mock_signal_handler.h"

static const FlutterSemanticsNode kBatchEndNode = {
    .id = kFlutterSemanticsNodeIdBatchEnd};

TEST(FlViewAccessibleTest, BuildTree) {
  g_autoptr(FlEngine) engine = make_mock_engine();
  g_autoptr(FlViewAccessible) accessible = FL_VIEW_ACCESSIBLE(
      g_object_new(fl_view_accessible_get_type(), "engine", engine, nullptr));

  const int32_t children[] = {111, 222};
  const FlutterSemanticsNode root_node = {
      .id = 0,
      .label = "root",
      .child_count = 2,
      .children_in_traversal_order = children,
  };
  fl_view_accessible_handle_update_semantics_node(accessible, &root_node);

  const FlutterSemanticsNode child1_node = {.id = 111, .label = "child 1"};
  fl_view_accessible_handle_update_semantics_node(accessible, &child1_node);

  const FlutterSemanticsNode child2_node = {.id = 222, .label = "child 2"};
  fl_view_accessible_handle_update_semantics_node(accessible, &child2_node);

  fl_view_accessible_handle_update_semantics_node(accessible, &kBatchEndNode);

  AtkObject* root_object =
      atk_object_ref_accessible_child(ATK_OBJECT(accessible), 0);
  EXPECT_STREQ(atk_object_get_name(root_object), "root");
  EXPECT_EQ(atk_object_get_index_in_parent(root_object), 0);
  EXPECT_EQ(atk_object_get_n_accessible_children(root_object), 2);

  AtkObject* child1_object = atk_object_ref_accessible_child(root_object, 0);
  EXPECT_STREQ(atk_object_get_name(child1_object), "child 1");
  EXPECT_EQ(atk_object_get_parent(child1_object), root_object);
  EXPECT_EQ(atk_object_get_index_in_parent(child1_object), 0);
  EXPECT_EQ(atk_object_get_n_accessible_children(child1_object), 0);

  AtkObject* child2_object = atk_object_ref_accessible_child(root_object, 1);
  EXPECT_STREQ(atk_object_get_name(child2_object), "child 2");
  EXPECT_EQ(atk_object_get_parent(child2_object), root_object);
  EXPECT_EQ(atk_object_get_index_in_parent(child2_object), 1);
  EXPECT_EQ(atk_object_get_n_accessible_children(child2_object), 0);
}

TEST(FlViewAccessibleTest, AddRemoveChildren) {
  g_autoptr(FlEngine) engine = make_mock_engine();
  g_autoptr(FlViewAccessible) accessible = FL_VIEW_ACCESSIBLE(
      g_object_new(fl_view_accessible_get_type(), "engine", engine, nullptr));

  FlutterSemanticsNode root_node = {
      .id = 0,
      .label = "root",
      .child_count = 0,
  };
  fl_view_accessible_handle_update_semantics_node(accessible, &root_node);

  fl_view_accessible_handle_update_semantics_node(accessible, &kBatchEndNode);

  AtkObject* root_object =
      atk_object_ref_accessible_child(ATK_OBJECT(accessible), 0);
  EXPECT_EQ(atk_object_get_n_accessible_children(root_object), 0);

  // add child1
  AtkObject* child1_object = nullptr;
  {
    flutter::testing::MockSignalHandler2<gint, AtkObject*> child1_added(
        root_object, "children-changed::add");
    EXPECT_SIGNAL2(child1_added, ::testing::Eq(0), ::testing::A<AtkObject*>())
        .WillOnce(::testing::SaveArg<1>(&child1_object));

    const int32_t children[] = {111};
    root_node.child_count = 1;
    root_node.children_in_traversal_order = children;
    fl_view_accessible_handle_update_semantics_node(accessible, &root_node);

    const FlutterSemanticsNode child1_node = {.id = 111, .label = "child 1"};
    fl_view_accessible_handle_update_semantics_node(accessible, &child1_node);

    fl_view_accessible_handle_update_semantics_node(accessible, &kBatchEndNode);
  }

  EXPECT_EQ(atk_object_get_n_accessible_children(root_object), 1);
  EXPECT_EQ(atk_object_ref_accessible_child(root_object, 0), child1_object);

  EXPECT_STREQ(atk_object_get_name(child1_object), "child 1");
  EXPECT_EQ(atk_object_get_parent(child1_object), root_object);
  EXPECT_EQ(atk_object_get_index_in_parent(child1_object), 0);
  EXPECT_EQ(atk_object_get_n_accessible_children(child1_object), 0);

  // add child2
  AtkObject* child2_object = nullptr;
  {
    flutter::testing::MockSignalHandler2<gint, AtkObject*> child2_added(
        root_object, "children-changed::add");
    EXPECT_SIGNAL2(child2_added, ::testing::Eq(1), ::testing::A<AtkObject*>())
        .WillOnce(::testing::SaveArg<1>(&child2_object));

    const int32_t children[] = {111, 222};
    root_node.child_count = 2;
    root_node.children_in_traversal_order = children;
    fl_view_accessible_handle_update_semantics_node(accessible, &root_node);

    const FlutterSemanticsNode child2_node = {.id = 222, .label = "child 2"};
    fl_view_accessible_handle_update_semantics_node(accessible, &child2_node);

    fl_view_accessible_handle_update_semantics_node(accessible, &kBatchEndNode);
  }

  EXPECT_EQ(atk_object_get_n_accessible_children(root_object), 2);
  EXPECT_EQ(atk_object_ref_accessible_child(root_object, 0), child1_object);
  EXPECT_EQ(atk_object_ref_accessible_child(root_object, 1), child2_object);

  EXPECT_STREQ(atk_object_get_name(child1_object), "child 1");
  EXPECT_EQ(atk_object_get_parent(child1_object), root_object);
  EXPECT_EQ(atk_object_get_index_in_parent(child1_object), 0);
  EXPECT_EQ(atk_object_get_n_accessible_children(child1_object), 0);

  EXPECT_STREQ(atk_object_get_name(child2_object), "child 2");
  EXPECT_EQ(atk_object_get_parent(child2_object), root_object);
  EXPECT_EQ(atk_object_get_index_in_parent(child2_object), 1);
  EXPECT_EQ(atk_object_get_n_accessible_children(child2_object), 0);

  // remove child1
  {
    flutter::testing::MockSignalHandler2<gint, AtkObject*> child1_removed(
        root_object, "children-changed::remove");
    EXPECT_SIGNAL2(child1_removed, ::testing::Eq(0),
                   ::testing::Eq(child1_object));

    const int32_t children[] = {222};
    root_node.child_count = 1;
    root_node.children_in_traversal_order = children;
    fl_view_accessible_handle_update_semantics_node(accessible, &root_node);

    fl_view_accessible_handle_update_semantics_node(accessible, &kBatchEndNode);
  }

  EXPECT_EQ(atk_object_get_n_accessible_children(root_object), 1);
  EXPECT_EQ(atk_object_ref_accessible_child(root_object, 0), child2_object);

  EXPECT_STREQ(atk_object_get_name(child2_object), "child 2");
  EXPECT_EQ(atk_object_get_parent(child2_object), root_object);
  EXPECT_EQ(atk_object_get_index_in_parent(child2_object), 0);
  EXPECT_EQ(atk_object_get_n_accessible_children(child2_object), 0);

  // remove child2
  {
    flutter::testing::MockSignalHandler2<gint, AtkObject*> child2_removed(
        root_object, "children-changed::remove");
    EXPECT_SIGNAL2(child2_removed, ::testing::Eq(0),
                   ::testing::Eq(child2_object));

    root_node.child_count = 0;
    fl_view_accessible_handle_update_semantics_node(accessible, &root_node);

    fl_view_accessible_handle_update_semantics_node(accessible, &kBatchEndNode);
  }

  EXPECT_EQ(atk_object_get_n_accessible_children(root_object), 0);
}
