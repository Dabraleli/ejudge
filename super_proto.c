/* -*- mode: c -*- */
/* $Id$ */

/* Copyright (C) 2004-2011 Alexander Chernov <cher@ejudge.ru> */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "super_proto.h"

#include "reuse_xalloc.h"

#include <stdio.h>

static unsigned char const * const error_map[] =
{
  "no error",
  "error code 1",
  "not connected",
  "invalid file descriptors",
  "write to server failed",
  "invalid socket name",
  "system call failed",
  "connection refused",
  "read from server failed",
  "unexpected EOF from server",
  "protocol error",
  "userlist-server is down",
  "permission denied",
  "invalid contest",
  "IP-address is banned",
  "contest root_dir is not set",
  "file does not exist",
  "log file is redirected to /dev/null",
  "read error",
  "file format is invalid",
  "unexpected userlist-server error",
  "contest is already used",
  "another contest is edited in this session",
  "not implemented yet",
  "invalid parameter",
  "no contest is edited",
  "duplicated login name",
  "such problem already exists",
  "this problem is used as base problem",
  "parameter is out of range",
  "request disabled in slave mode",

  "unknown error",
};

unsigned char const *
super_proto_strerror(int n)
{
  if (n < 0) n = -n;
  if (n >= SSERV_ERR_LAST) {
    // this is error anyway, so leak some memory
    unsigned char buf[64];

    snprintf(buf, sizeof(buf), "unknown error %d", n);
    return xstrdup(buf);
  }
  return error_map[n];
}

const unsigned char * const super_proto_op_names[SSERV_OP_LAST] =
{
  [SSERV_OP_VIEW_CNTS_DETAILS] = "VIEW_CNTS_DETAILS",
  [SSERV_OP_EDITED_CNTS_BACK] = "EDITED_CNTS_BACK",
  [SSERV_OP_EDITED_CNTS_CONTINUE] = "EDITED_CNTS_CONTINUE",
  [SSERV_OP_EDITED_CNTS_START_NEW] = "EDITED_CNTS_START_NEW",
  [SSERV_OP_LOCKED_CNTS_FORGET] = "LOCKED_CNTS_FORGET",
  [SSERV_OP_LOCKED_CNTS_CONTINUE] = "LOCKED_CNTS_CONTINUE",
  [SSERV_OP_EDIT_CONTEST_PAGE] = "EDIT_CONTEST_PAGE",
  [SSERV_OP_EDIT_CONTEST_PAGE_2] = "EDIT_CONTEST_PAGE_2",
  [SSERV_OP_CLEAR_CONTEST_XML_FIELD] = "CLEAR_CONTEST_XML_FIELD",
  [SSERV_OP_EDIT_CONTEST_XML_FIELD] = "EDIT_CONTEST_XML_FIELD",
  [SSERV_OP_TOGGLE_CONTEST_XML_VISIBILITY] = "TOGGLE_CONTEST_XML_VISIBILITY",
  [SSERV_OP_CONTEST_XML_FIELD_EDIT_PAGE] = "CONTEST_XML_FIELD_EDIT_PAGE",
  [SSERV_OP_SAVE_FILE_CONTEST_XML] = "SAVE_FILE_CONTEST_XML",
  [SSERV_OP_CLEAR_FILE_CONTEST_XML] = "CLEAR_FILE_CONTEST_XML",
  [SSERV_OP_RELOAD_FILE_CONTEST_XML] = "RELOAD_FILE_CONTEST_XML",
  [SSERV_OP_COPY_ACCESS_RULES_PAGE] = "COPY_ACCESS_RULES_PAGE",
  [SSERV_OP_COPY_ALL_ACCESS_RULES_PAGE] = "COPY_ALL_ACCESS_RULES_PAGE",
  [SSERV_OP_COPY_ALL_ACCESS_RULES] = "COPY_ALL_ACCESS_RULES",
  [SSERV_OP_COPY_ALL_PRIV_USERS_PAGE] = "COPY_ALL_PRIV_USERS_PAGE",
  [SSERV_OP_COPY_ALL_PRIV_USERS] = "COPY_ALL_PRIV_USERS",
  [SSERV_OP_ADD_PRIV_USER] = "ADD_PRIV_USER",
  [SSERV_OP_EDIT_PERMISSIONS_PAGE] = "EDIT_PERMISSIONS_PAGE",
  [SSERV_OP_EDIT_GENERAL_FIELDS_PAGE] = "EDIT_GENERAL_FIELDS_PAGE",
  [SSERV_OP_EDIT_MEMBER_FIELDS_PAGE] = "EDIT_MEMBER_FIELDS_PAGE",
  [SSERV_OP_DELETE_PRIV_USER] = "DELETE_PRIV_USER",
  [SSERV_OP_SET_PREDEF_PRIV] = "SET_PREDEF_PRIV",
  [SSERV_OP_SET_PRIV] = "SET_PRIV",
  [SSERV_OP_SET_DEFAULT_ACCESS] = "SET_DEFAULT_ACCESS",
  [SSERV_OP_CHECK_IP_MASK] = "CHECK_IP_MASK",
  [SSERV_OP_ADD_IP] = "ADD_IP",
  [SSERV_OP_SET_RULE_ACCESS] = "SET_RULE_ACCESS",
  [SSERV_OP_SET_RULE_SSL] = "SET_RULE_SSL",
  [SSERV_OP_SET_RULE_IP] = "SET_RULE_IP",
  [SSERV_OP_DELETE_RULE] = "DELETE_RULE",
  [SSERV_OP_FORWARD_RULE] = "FORWARD_RULE",
  [SSERV_OP_BACKWARD_RULE] = "BACKWARD_RULE",
  [SSERV_OP_COPY_ACCESS_RULES] = "COPY_ACCESS_RULES",
  [SSERV_OP_EDIT_GENERAL_FIELDS] = "EDIT_GENERAL_FIELDS",
  [SSERV_OP_EDIT_MEMBER_FIELDS] = "EDIT_MEMBER_FIELDS",
  [SSERV_OP_CREATE_NEW_CONTEST_PAGE] = "CREATE_NEW_CONTEST_PAGE",
  [SSERV_OP_CREATE_NEW_CONTEST] = "CREATE_NEW_CONTEST",
  [SSERV_OP_FORGET_CONTEST] = "FORGET_CONTEST",
  [SSERV_OP_EDIT_SERVE_GLOBAL_FIELD] = "EDIT_SERVE_GLOBAL_FIELD",
  [SSERV_OP_CLEAR_SERVE_GLOBAL_FIELD] = "CLEAR_SERVE_GLOBAL_FIELD",
  [SSERV_OP_EDIT_SID_STATE_FIELD] = "EDIT_SID_STATE_FIELD",
  [SSERV_OP_EDIT_SID_STATE_FIELD_NEGATED] = "EDIT_SID_STATE_FIELD_NEGATED",
  [SSERV_OP_EDIT_SERVE_GLOBAL_FIELD_DETAIL_PAGE] = "EDIT_SERVE_GLOBAL_FIELD_DETAIL_PAGE",
  [SSERV_OP_EDIT_SERVE_GLOBAL_FIELD_DETAIL] = "EDIT_SERVE_GLOBAL_FIELD_DETAIL",
  [SSERV_OP_SET_SID_STATE_LANG_FIELD] = "SET_SID_STATE_LANG_FIELD",
  [SSERV_OP_CLEAR_SID_STATE_LANG_FIELD] = "CLEAR_SID_STATE_LANG_FIELD",
  [SSERV_OP_SET_SERVE_LANG_FIELD] = "SET_SERVE_LANG_FIELD",
  [SSERV_OP_CLEAR_SERVE_LANG_FIELD] = "CLEAR_SERVE_LANG_FIELD",
  [SSERV_OP_EDIT_SERVE_LANG_FIELD_DETAIL_PAGE] = "EDIT_SERVE_LANG_FIELD_DETAIL_PAGE",
  [SSERV_OP_EDIT_SERVE_LANG_FIELD_DETAIL] = "EDIT_SERVE_LANG_FIELD_DETAIL",
  [SSERV_OP_SERVE_LANG_UPDATE_VERSIONS] = "SERVE_LANG_UPDATE_VERSIONS",
  [SSERV_OP_CREATE_ABSTR_PROB] = "CREATE_ABSTR_PROB",
  [SSERV_OP_CREATE_CONCRETE_PROB] = "CREATE_CONCRETE_PROB",
  [SSERV_OP_DELETE_PROB] = "DELETE_PROB",
  [SSERV_OP_SET_SID_STATE_PROB_FIELD] = "SET_SID_STATE_PROB_FIELD",
  [SSERV_OP_SET_SERVE_PROB_FIELD] = "SET_SERVE_PROB_FIELD",
  [SSERV_OP_CLEAR_SERVE_PROB_FIELD] = "CLEAR_SERVE_PROB_FIELD",
  [SSERV_OP_EDIT_SERVE_PROB_FIELD_DETAIL_PAGE] = "EDIT_SERVE_PROB_FIELD_DETAIL_PAGE",
  [SSERV_OP_EDIT_SERVE_PROB_FIELD_DETAIL] = "EDIT_SERVE_PROB_FIELD_DETAIL",
  [SSERV_OP_BROWSE_PROBLEM_PACKAGES] = "BROWSE_PROBLEM_PACKAGES",
  [SSERV_OP_CREATE_PACKAGE] = "CREATE_PACKAGE",
  [SSERV_OP_CREATE_PROBLEM] = "CREATE_PROBLEM",
  [SSERV_OP_DELETE_ITEM] = "DELETE_ITEM",
  [SSERV_OP_EDIT_PROBLEM] = "EDIT_PROBLEM",
  [SSERV_OP_USER_BROWSE_PAGE] = "USER_BROWSE_PAGE",
  [SSERV_OP_USER_FILTER_CHANGE_ACTION] = "USER_FILTER_CHANGE_ACTION",
  [SSERV_OP_USER_FILTER_FIRST_PAGE_ACTION] = "USER_FILTER_FIRST_PAGE_ACTION",
  [SSERV_OP_USER_FILTER_PREV_PAGE_ACTION] = "USER_FILTER_PREV_PAGE_ACTION",
  [SSERV_OP_USER_FILTER_NEXT_PAGE_ACTION] = "USER_FILTER_NEXT_PAGE_ACTION",
  [SSERV_OP_USER_FILTER_LAST_PAGE_ACTION] = "USER_FILTER_LAST_PAGE_ACTION",
  [SSERV_OP_USER_JUMP_CONTEST_ACTION] = "USER_JUMP_CONTEST_ACTION",
  [SSERV_OP_USER_JUMP_GROUP_ACTION] = "USER_JUMP_GROUP_ACTION",
  [SSERV_OP_USER_BROWSE_MARK_ALL_ACTION] = "USER_BROWSE_MARK_ALL_ACTION",
  [SSERV_OP_USER_BROWSE_UNMARK_ALL_ACTION] = "USER_BROWSE_UNMARK_ALL_ACTION",
  [SSERV_OP_USER_BROWSE_TOGGLE_ALL_ACTION] = "USER_BROWSE_TOGGLE_ALL_ACTION",
  [SSERV_OP_USER_CREATE_ONE_PAGE] = "USER_CREATE_ONE_PAGE",
  [SSERV_OP_USER_CREATE_ONE_ACTION] = "USER_CREATE_ONE_ACTION",
  [SSERV_OP_USER_CREATE_MANY_PAGE] = "USER_CREATE_MANY_PAGE",
  [SSERV_OP_USER_CREATE_MANY_ACTION] = "USER_CREATE_MANY_ACTION",
  [SSERV_OP_USER_CREATE_FROM_CSV_PAGE] = "USER_CREATE_FROM_CSV_PAGE",
  [SSERV_OP_USER_CREATE_FROM_CSV_ACTION] = "USER_CREATE_FROM_CSV_ACTION",
  [SSERV_OP_USER_DETAIL_PAGE] = "USER_DETAIL_PAGE",
  [SSERV_OP_USER_PASSWORD_PAGE] = "USER_PASSWORD_PAGE",
  [SSERV_OP_USER_CHANGE_PASSWORD_ACTION] = "USER_CHANGE_PASSWORD_ACTION",
  [SSERV_OP_USER_CNTS_PASSWORD_PAGE] = "USER_CNTS_PASSWORD_PAGE",
  [SSERV_OP_USER_CHANGE_CNTS_PASSWORD_ACTION] = "USER_CHANGE_CNTS_PASSWORD_ACTION",
  [SSERV_OP_USER_CLEAR_FIELD_ACTION] = "USER_CLEAR_FIELD_ACTION",
  [SSERV_OP_USER_CREATE_MEMBER_ACTION] = "USER_CREATE_MEMBER_ACTION",
  [SSERV_OP_USER_DELETE_MEMBER_PAGE] = "USER_DELETE_MEMBER_PAGE",
  [SSERV_OP_USER_DELETE_MEMBER_ACTION] = "USER_DELETE_MEMBER_ACTION",
  [SSERV_OP_USER_SAVE_AND_PREV_ACTION] = "USER_SAVE_AND_PREV_ACTION",
  [SSERV_OP_USER_SAVE_ACTION] = "USER_SAVE_ACTION",
  [SSERV_OP_USER_SAVE_AND_NEXT_ACTION] = "USER_SAVE_AND_NEXT_ACTION",
  [SSERV_OP_USER_CANCEL_AND_PREV_ACTION] = "USER_CANCEL_AND_PREV_ACTION",
  [SSERV_OP_USER_CANCEL_ACTION] = "USER_CANCEL_ACTION",
  [SSERV_OP_USER_CANCEL_AND_NEXT_ACTION] = "USER_CANCEL_AND_NEXT_ACTION",
  [SSERV_OP_USER_CREATE_REG_PAGE] = "USER_CREATE_REG_PAGE",
  [SSERV_OP_USER_CREATE_REG_ACTION] = "USER_CREATE_REG_ACTION",
  [SSERV_OP_USER_EDIT_REG_PAGE] = "USER_EDIT_REG_PAGE",
  [SSERV_OP_USER_EDIT_REG_ACTION] = "USER_EDIT_REG_ACTION",
  [SSERV_OP_USER_DELETE_REG_PAGE] = "USER_DELETE_REG_PAGE",
  [SSERV_OP_USER_DELETE_REG_ACTION] = "USER_DELETE_REG_ACTION",
  [SSERV_OP_USER_DELETE_SESSION_ACTION] = "USER_DELETE_SESSION_ACTION",
  [SSERV_OP_USER_DELETE_ALL_SESSIONS_ACTION] = "USER_DELETE_ALL_SESSIONS_ACTION",
  [SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE] = "USER_SEL_RANDOM_PASSWD_PAGE",
  [SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION] = "USER_SEL_RANDOM_PASSWD_ACTION",
  [SSERV_OP_USER_SEL_CLEAR_CNTS_PASSWD_PAGE] = "USER_SEL_CLEAR_CNTS_PASSWD_PAGE",
  [SSERV_OP_USER_SEL_CLEAR_CNTS_PASSWD_ACTION] = "USER_SEL_CLEAR_CNTS_PASSWD_ACTION",
  [SSERV_OP_USER_SEL_RANDOM_CNTS_PASSWD_PAGE] = "USER_SEL_RANDOM_CNTS_PASSWD_PAGE",
  [SSERV_OP_USER_SEL_RANDOM_CNTS_PASSWD_ACTION] = "USER_SEL_RANDOM_CNTS_PASSWD_ACTION",
  [SSERV_OP_USER_SEL_CREATE_REG_PAGE] = "USER_SEL_CREATE_REG_PAGE",
  [SSERV_OP_USER_SEL_CREATE_REG_ACTION] = "USER_SEL_CREATE_REG_ACTION",
  [SSERV_OP_USER_SEL_CREATE_REG_AND_COPY_PAGE] = "USER_SEL_CREATE_REG_AND_COPY_PAGE",
  [SSERV_OP_USER_SEL_CREATE_REG_AND_COPY_ACTION] = "USER_SEL_CREATE_REG_AND_COPY_ACTION",
  [SSERV_OP_USER_SEL_DELETE_REG_PAGE] = "USER_SEL_DELETE_REG_PAGE",
  [SSERV_OP_USER_SEL_DELETE_REG_ACTION] = "USER_SEL_DELETE_REG_ACTION",
  [SSERV_OP_USER_SEL_CHANGE_REG_STATUS_PAGE] = "USER_SEL_CHANGE_REG_STATUS_PAGE",
  [SSERV_OP_USER_SEL_CHANGE_REG_STATUS_ACTION] = "USER_SEL_CHANGE_REG_STATUS_ACTION",
  [SSERV_OP_USER_SEL_CHANGE_REG_FLAGS_PAGE] = "USER_SEL_CHANGE_REG_FLAGS_PAGE",
  [SSERV_OP_USER_SEL_CHANGE_REG_FLAGS_ACTION] = "USER_SEL_CHANGE_REG_FLAGS_ACTION",
  [SSERV_OP_USER_SEL_CANCEL_ACTION] = "USER_SEL_CANCEL_ACTION",
  [SSERV_OP_USER_SEL_VIEW_PASSWD_PAGE] = "USER_SEL_VIEW_PASSWD_PAGE",
  [SSERV_OP_USER_SEL_VIEW_CNTS_PASSWD_PAGE] = "USER_SEL_VIEW_CNTS_PASSWD_PAGE",
  [SSERV_OP_USER_SEL_VIEW_PASSWD_REDIRECT] = "USER_SEL_VIEW_PASSWD_REDIRECT",
  [SSERV_OP_USER_SEL_VIEW_CNTS_PASSWD_REDIRECT] = "USER_SEL_VIEW_CNTS_PASSWD_REDIRECT",
  [SSERV_OP_USER_SEL_CREATE_GROUP_MEMBER_PAGE] = "USER_SEL_CREATE_GROUP_MEMBER_PAGE",
  [SSERV_OP_USER_SEL_CREATE_GROUP_MEMBER_ACTION] = "USER_SEL_CREATE_GROUP_MEMBER_ACTION",
  [SSERV_OP_USER_SEL_DELETE_GROUP_MEMBER_PAGE] = "USER_SEL_DELETE_GROUP_MEMBER_PAGE",
  [SSERV_OP_USER_SEL_DELETE_GROUP_MEMBER_ACTION] = "USER_SEL_DELETE_GROUP_MEMBER_ACTION",
  [SSERV_OP_USER_IMPORT_CSV_PAGE] = "USER_IMPORT_CSV_PAGE",
  [SSERV_OP_USER_IMPORT_CSV_ACTION] = "USER_IMPORT_CSV_ACTION",
  [SSERV_OP_GROUP_BROWSE_PAGE] = "GROUP_BROWSE_PAGE",
  [SSERV_OP_GROUP_FILTER_CHANGE_ACTION] = "GROUP_FILTER_CHANGE_ACTION",
  [SSERV_OP_GROUP_FILTER_FIRST_PAGE_ACTION] = "GROUP_FILTER_FIRST_PAGE_ACTION",
  [SSERV_OP_GROUP_FILTER_PREV_PAGE_ACTION] = "GROUP_FILTER_PREV_PAGE_ACTION",
  [SSERV_OP_GROUP_FILTER_NEXT_PAGE_ACTION] = "GROUP_FILTER_NEXT_PAGE_ACTION",
  [SSERV_OP_GROUP_FILTER_LAST_PAGE_ACTION] = "GROUP_FILTER_LAST_PAGE_ACTION",
  [SSERV_OP_GROUP_CREATE_PAGE] = "GROUP_CREATE_PAGE",
  [SSERV_OP_GROUP_CREATE_ACTION] = "GROUP_CREATE_ACTION",
  [SSERV_OP_GROUP_MODIFY_PAGE] = "GROUP_MODIFY_PAGE",
  [SSERV_OP_GROUP_MODIFY_PAGE_ACTION] = "GROUP_MODIFY_PAGE_ACTION",
  [SSERV_OP_GROUP_MODIFY_ACTION] = "GROUP_MODIFY_ACTION",
  [SSERV_OP_GROUP_DELETE_PAGE] = "GROUP_DELETE_PAGE",
  [SSERV_OP_GROUP_DELETE_PAGE_ACTION] = "GROUP_DELETE_PAGE_ACTION",
  [SSERV_OP_GROUP_DELETE_ACTION] = "GROUP_DELETE_ACTION",
  [SSERV_OP_GROUP_CANCEL_ACTION] = "GROUP_CANCEL_ACTION",

  [SSERV_OP_TESTS_MAIN_PAGE] = "TESTS_MAIN_PAGE",
  [SSERV_OP_TESTS_STATEMENT_EDIT_PAGE] = "TESTS_STATEMENT_EDIT_PAGE",
  [SSERV_OP_TESTS_STATEMENT_EDIT_ACTION] = "TESTS_STATEMENT_EDIT_ACTION",
  [SSERV_OP_TESTS_STATEMENT_EDIT_2_ACTION] = "TESTS_STATEMENT_EDIT_2_ACTION",
  [SSERV_OP_TESTS_STATEMENT_EDIT_3_ACTION] = "TESTS_STATEMENT_EDIT_3_ACTION",
  [SSERV_OP_TESTS_STATEMENT_EDIT_4_ACTION] = "TESTS_STATEMENT_EDIT_4_ACTION",
  [SSERV_OP_TESTS_STATEMENT_EDIT_5_ACTION] = "TESTS_STATEMENT_EDIT_5_ACTION",
  [SSERV_OP_TESTS_STATEMENT_DELETE_ACTION] = "TESTS_STATEMENT_DELETE_ACTION",
  [SSERV_OP_TESTS_STATEMENT_DELETE_SAMPLE_ACTION] = "TESTS_STATEMENT_DELETE_SAMPLE_ACTION",
  [SSERV_OP_TESTS_TESTS_VIEW_PAGE] = "TESTS_TESTS_VIEW_PAGE",
  [SSERV_OP_TESTS_SOURCE_HEADER_EDIT_PAGE] = "TESTS_SOURCE_HEADER_EDIT_PAGE",
  [SSERV_OP_TESTS_SOURCE_HEADER_EDIT_ACTION] = "TESTS_SOURCE_HEADER_EDIT_ACTION",
  [SSERV_OP_TESTS_SOURCE_HEADER_DELETE_ACTION] = "TESTS_SOURCE_HEADER_DELETE_ACTION",
  [SSERV_OP_TESTS_SOURCE_FOOTER_EDIT_PAGE] = "TESTS_SOURCE_FOOTER_EDIT_PAGE",
  [SSERV_OP_TESTS_SOURCE_FOOTER_EDIT_ACTION] = "TESTS_SOURCE_FOOTER_EDIT_ACTION",
  [SSERV_OP_TESTS_SOURCE_FOOTER_DELETE_ACTION] = "TESTS_SOURCE_FOOTER_DELETE_ACTION",
  [SSERV_OP_TESTS_SOLUTION_EDIT_PAGE] = "TESTS_SOLUTION_EDIT_PAGE",
  [SSERV_OP_TESTS_SOLUTION_EDIT_ACTION] = "TESTS_SOLUTION_EDIT_ACTION",
  [SSERV_OP_TESTS_SOLUTION_DELETE_ACTION] = "TESTS_SOLUTION_DELETE_ACTION",
  [SSERV_OP_TESTS_STYLE_CHECKER_EDIT_PAGE] = "TESTS_STYLE_CHECKER_EDIT_PAGE",
  [SSERV_OP_TESTS_CHECKER_EDIT_PAGE] = "TESTS_CHECKER_EDIT_PAGE",
  [SSERV_OP_TESTS_VALUER_EDIT_PAGE] = "TESTS_VALUER_EDIT_PAGE",
  [SSERV_OP_TESTS_INTERACTOR_EDIT_PAGE] = "TESTS_INTERACTOR_EDIT_PAGE",
  [SSERV_OP_TESTS_TEST_CHECKER_EDIT_PAGE] = "TESTS_TEST_CHECKER_EDIT_PAGE",
  [SSERV_OP_TESTS_MAKEFILE_EDIT_PAGE] = "TESTS_MAKEFILE_EDIT_PAGE",
  [SSERV_OP_TESTS_MAKEFILE_EDIT_ACTION] = "TESTS_MAKEFILE_EDIT_ACTION",
  [SSERV_OP_TESTS_MAKEFILE_DELETE_ACTION] = "TESTS_MAKEFILE_DELETE_ACTION",
  [SSERV_OP_TESTS_MAKEFILE_GENERATE_ACTION] = "TESTS_MAKEFILE_GENERATE_ACTION",
  [SSERV_OP_TESTS_TEST_MOVE_UP_ACTION] = "TESTS_TEST_MOVE_UP_ACTION",
  [SSERV_OP_TESTS_TEST_MOVE_DOWN_ACTION] = "TESTS_TEST_MOVE_DOWN_ACTION",
  [SSERV_OP_TESTS_TEST_MOVE_TO_SAVED_ACTION] = "TESTS_TEST_MOVE_TO_SAVED_ACTION",
  [SSERV_OP_TESTS_TEST_INSERT_PAGE] =  "TESTS_TEST_INSERT_PAGE",
  [SSERV_OP_TESTS_TEST_INSERT_ACTION] =  "TESTS_TEST_INSERT_ACTION",
  [SSERV_OP_TESTS_TEST_EDIT_PAGE] = "TESTS_TEST_EDIT_PAGE",
  [SSERV_OP_TESTS_TEST_EDIT_ACTION] = "TESTS_TEST_EDIT_ACTION",
  [SSERV_OP_TESTS_TEST_DELETE_PAGE] = "TESTS_TEST_DELETE_PAGE",
  [SSERV_OP_TESTS_TEST_DELETE_ACTION] = "TESTS_TEST_DELETE_ACTION",
  [SSERV_OP_TESTS_TEST_UPLOAD_ARCHIVE_1_PAGE] = "TESTS_TEST_UPLOAD_ARCHIVE_1_PAGE",
  [SSERV_OP_TESTS_SAVED_MOVE_UP_ACTION] = "TESTS_SAVED_MOVE_UP_ACTION",
  [SSERV_OP_TESTS_SAVED_MOVE_DOWN_ACTION] = "TESTS_SAVED_MOVE_DOWN_ACTION",
  [SSERV_OP_TESTS_SAVED_DELETE_PAGE] = "TESTS_SAVED_DELETE_PAGE",
  [SSERV_OP_TESTS_SAVED_MOVE_TO_TEST_ACTION] = "TESTS_SAVED_MOVE_TO_TEST_ACTION",
  [SSERV_OP_TESTS_README_CREATE_PAGE] = "TESTS_README_CREATE_PAGE",
  [SSERV_OP_TESTS_README_EDIT_PAGE] = "TESTS_README_EDIT_PAGE",
  [SSERV_OP_TESTS_README_DELETE_PAGE] = "TESTS_README_DELETE_PAGE",
  [SSERV_OP_TESTS_CANCEL_ACTION] = "TESTS_CANCEL_ACTION",
  [SSERV_OP_TESTS_CANCEL_2_ACTION] = "TESTS_CANCEL_2_ACTION",
  [SSERV_OP_TESTS_TEST_DOWNLOAD] = "TESTS_TEST_DOWNLOAD",
  [SSERV_OP_TESTS_TEST_UPLOAD_PAGE] = "TESTS_TEST_UPLOAD_PAGE",
  [SSERV_OP_TESTS_TEST_CLEAR_INF_ACTION] = "TESTS_TEST_CLEAR_INF_ACTION",

};

const int super_proto_op_redirect[SSERV_OP_LAST] =
{
  [SSERV_OP_USER_FILTER_FIRST_PAGE_ACTION] = SSERV_OP_USER_FILTER_CHANGE_ACTION,
  [SSERV_OP_USER_FILTER_PREV_PAGE_ACTION] = SSERV_OP_USER_FILTER_CHANGE_ACTION,
  [SSERV_OP_USER_FILTER_NEXT_PAGE_ACTION] = SSERV_OP_USER_FILTER_CHANGE_ACTION,
  [SSERV_OP_USER_FILTER_LAST_PAGE_ACTION] = SSERV_OP_USER_FILTER_CHANGE_ACTION,

  [SSERV_OP_USER_BROWSE_UNMARK_ALL_ACTION] = SSERV_OP_USER_BROWSE_MARK_ALL_ACTION,
  [SSERV_OP_USER_BROWSE_TOGGLE_ALL_ACTION] = SSERV_OP_USER_BROWSE_MARK_ALL_ACTION,

  [SSERV_OP_USER_SAVE_AND_PREV_ACTION] = SSERV_OP_USER_SAVE_ACTION,
  [SSERV_OP_USER_SAVE_AND_NEXT_ACTION] = SSERV_OP_USER_SAVE_ACTION,

  [SSERV_OP_USER_CANCEL_AND_PREV_ACTION] = SSERV_OP_USER_CANCEL_ACTION,
  [SSERV_OP_USER_CANCEL_AND_NEXT_ACTION] = SSERV_OP_USER_CANCEL_ACTION,

  [SSERV_OP_USER_SEL_CLEAR_CNTS_PASSWD_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_RANDOM_CNTS_PASSWD_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_DELETE_REG_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_CHANGE_REG_STATUS_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_CHANGE_REG_FLAGS_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_CREATE_REG_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_CREATE_REG_AND_COPY_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_CREATE_GROUP_MEMBER_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_DELETE_GROUP_MEMBER_PAGE] = SSERV_OP_USER_SEL_RANDOM_PASSWD_PAGE,

  [SSERV_OP_USER_SEL_CLEAR_CNTS_PASSWD_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_RANDOM_CNTS_PASSWD_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_DELETE_REG_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_CHANGE_REG_STATUS_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_CHANGE_REG_FLAGS_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_CREATE_REG_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_CREATE_REG_AND_COPY_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_CREATE_GROUP_MEMBER_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,
  [SSERV_OP_USER_SEL_DELETE_GROUP_MEMBER_ACTION] = SSERV_OP_USER_SEL_RANDOM_PASSWD_ACTION,

  [SSERV_OP_USER_SEL_VIEW_CNTS_PASSWD_PAGE] = SSERV_OP_USER_SEL_VIEW_PASSWD_PAGE,
  [SSERV_OP_USER_SEL_VIEW_CNTS_PASSWD_REDIRECT] = SSERV_OP_USER_SEL_VIEW_PASSWD_REDIRECT,

  [SSERV_OP_GROUP_FILTER_FIRST_PAGE_ACTION] = SSERV_OP_GROUP_FILTER_CHANGE_ACTION,
  [SSERV_OP_GROUP_FILTER_PREV_PAGE_ACTION] = SSERV_OP_GROUP_FILTER_CHANGE_ACTION,
  [SSERV_OP_GROUP_FILTER_NEXT_PAGE_ACTION] = SSERV_OP_GROUP_FILTER_CHANGE_ACTION,
  [SSERV_OP_GROUP_FILTER_LAST_PAGE_ACTION] = SSERV_OP_GROUP_FILTER_CHANGE_ACTION,

  [SSERV_OP_TESTS_TEST_MOVE_DOWN_ACTION] = SSERV_OP_TESTS_TEST_MOVE_UP_ACTION,
  [SSERV_OP_TESTS_SAVED_MOVE_UP_ACTION] = SSERV_OP_TESTS_TEST_MOVE_UP_ACTION,
  [SSERV_OP_TESTS_SAVED_MOVE_DOWN_ACTION] = SSERV_OP_TESTS_TEST_MOVE_UP_ACTION,
  [SSERV_OP_TESTS_SAVED_MOVE_TO_TEST_ACTION] = SSERV_OP_TESTS_TEST_MOVE_TO_SAVED_ACTION,

  [SSERV_OP_TESTS_STATEMENT_EDIT_3_ACTION] = SSERV_OP_TESTS_STATEMENT_EDIT_ACTION,
  [SSERV_OP_TESTS_STATEMENT_EDIT_4_ACTION] = SSERV_OP_TESTS_STATEMENT_EDIT_ACTION,
  [SSERV_OP_TESTS_STATEMENT_DELETE_SAMPLE_ACTION] = SSERV_OP_TESTS_STATEMENT_EDIT_ACTION,

  [SSERV_OP_TESTS_STATEMENT_EDIT_5_ACTION] = SSERV_OP_TESTS_STATEMENT_EDIT_2_ACTION,

  [SSERV_OP_TESTS_SOURCE_FOOTER_EDIT_PAGE] = SSERV_OP_TESTS_SOURCE_HEADER_EDIT_PAGE,
  [SSERV_OP_TESTS_SOURCE_FOOTER_EDIT_ACTION] = SSERV_OP_TESTS_SOURCE_HEADER_EDIT_ACTION,
  [SSERV_OP_TESTS_SOURCE_FOOTER_DELETE_ACTION] = SSERV_OP_TESTS_SOURCE_HEADER_DELETE_ACTION,
  [SSERV_OP_TESTS_SOLUTION_EDIT_PAGE] = SSERV_OP_TESTS_SOURCE_HEADER_EDIT_PAGE,
  [SSERV_OP_TESTS_SOLUTION_EDIT_ACTION] = SSERV_OP_TESTS_SOURCE_HEADER_EDIT_ACTION,
  [SSERV_OP_TESTS_SOLUTION_DELETE_ACTION] = SSERV_OP_TESTS_SOURCE_HEADER_DELETE_ACTION,

  [SSERV_OP_TESTS_TEST_INSERT_PAGE] = SSERV_OP_TESTS_TEST_EDIT_PAGE,
  [SSERV_OP_TESTS_TEST_INSERT_ACTION] = SSERV_OP_TESTS_TEST_EDIT_ACTION,

  [SSERV_OP_TESTS_CANCEL_2_ACTION] = SSERV_OP_TESTS_CANCEL_ACTION,
};

unsigned char const * const super_proto_op_error_messages[] =
{
  [S_ERR_EMPTY_REPLY] = "Reply text is empty",
  [S_ERR_INV_OPER] = "Invalid operation",
  [S_ERR_CONTEST_EDITED] = "Cannot edit more than one contest at a time",
  [S_ERR_INV_SID] = "Invalid session id",
  [S_ERR_INV_CONTEST] = "Invalid contest id",
  [S_ERR_PERM_DENIED] = "Permission denied",
  [S_ERR_INTERNAL] = "Internal error",
  [S_ERR_ALREADY_EDITED] = "Contest is already edited",
  [S_ERR_NO_EDITED_CNTS] = "No contest is edited",
  [S_ERR_INV_FIELD_ID] = "Invalid field ID",
  [S_ERR_NOT_IMPLEMENTED] = "Not implemented yet",
  [S_ERR_INV_VALUE] = "Invalid value",
  [S_ERR_CONTEST_ALREADY_EXISTS] = "Contest with this ID already exists",
  [S_ERR_CONTEST_ALREADY_EDITED] = "Contest is edited by another person",
  [S_ERR_INV_LANG_ID] = "Invalid Lang ID",
  [S_ERR_INV_PROB_ID] = "Invalid Prob ID",
  [S_ERR_INV_PACKAGE] = "Invalid package",
  [S_ERR_ITEM_EXISTS] = "Such item already exists",
  [S_ERR_OPERATION_FAILED] = "System operation failed",
  [S_ERR_INV_USER_ID] = "Invalid User ID",
  [S_ERR_NO_CONNECTION] = "No connection to the database",
  [S_ERR_DB_ERROR] = "Database error",
  [S_ERR_UNSPEC_PASSWD1] = "Password 1 is not specified",
  [S_ERR_UNSPEC_PASSWD2] = "Password 2 is not specified",
  [S_ERR_INV_PASSWD1] = "Password 1 is invalid",
  [S_ERR_INV_PASSWD2] = "Password 2 is invalid",
  [S_ERR_PASSWDS_DIFFER] = "Passwords do not match each other",
  [S_ERR_UNSPEC_LOGIN] = "Login is not specified",
  [S_ERR_DUPLICATED_LOGIN] = "This login is aready used",
  [S_ERR_INV_GROUP_ID] = "Invalid group ID",
  [S_ERR_INV_FIRST_SERIAL] = "Invalid first serial number",
  [S_ERR_INV_LAST_SERIAL] = "Invalid last serial number",
  [S_ERR_INV_RANGE] = "Invalid serial number range",
  [S_ERR_INV_LOGIN_TEMPLATE] = "Invalid login template",
  [S_ERR_INV_REG_PASSWORD_TEMPLATE] = "Invalid registration password template",
  [S_ERR_INV_CNTS_PASSWORD_TEMPLATE] = "Invalid contest password template",
  [S_ERR_INV_CNTS_NAME_TEMPLATE] = "Invalid name template",
  [S_ERR_INV_CSV_FILE] = "Invalid CSV file",
  [S_ERR_INV_CHARSET] = "Invalid charset",
  [S_ERR_INV_SEPARATOR] = "Invalid field separator",
  [S_ERR_DATA_READ_ONLY] = "Data is read-only",
  [S_ERR_TOO_MANY_MEMBERS] = "Too many members",
  [S_ERR_INV_SERIAL] = "Invalid member",
  [S_ERR_INV_EMAIL] = "Invalid email",
  [S_ERR_INV_GROUP_NAME] = "Invalid group name",
  [S_ERR_INV_DESCRIPTION] = "Invalid description",
  [S_ERR_GROUP_CREATION_FAILED] = "Group creation failed",
  [S_ERR_INV_SERVE_CONFIG_PATH] = "Invalid serve.cfg configuration file",
  [S_ERR_INV_VARIANT] = "Invalid variant",
  [S_ERR_UNSUPPORTED_SETTINGS] = "Settings of this contest are incompatible with this feature",
  [S_ERR_INV_CNTS_SETTINGS] = "Invalid contest settings",
  [S_ERR_INV_TEST_NUM] = "Invalid test number",
  [S_ERR_FS_ERROR] = "Filesystem error",
  [S_ERR_INV_EXIT_CODE] = "Invalid exit code",
  [S_ERR_INV_SYS_GROUP] = "Invalid system group",
  [S_ERR_INV_SYS_MODE] = "Invalid system mode",
  [S_ERR_INV_TESTINFO] = "Invalid test information",
  [S_ERR_INV_PROB_XML] = "Invalid statement XML file",
  [S_ERR_UNSPEC_PROB_PACKAGE] = "Problem package is unspecified",
  [S_ERR_UNSPEC_PROB_NAME] = "Problem name is unspecified",
  [S_ERR_INV_XHTML] = "Invalid XHTML",
};

/*
 * Local variables:
 *  compile-command: "make"
 *  c-font-lock-extra-types: ("\\sw+_t" "FILE")
 * End:
 */
