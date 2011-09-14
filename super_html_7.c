/* -*- mode: c -*- */
/* $Id$ */

/* Copyright (C) 2011 Alexander Chernov <cher@ejudge.ru> */

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

#include "config.h"
#include "version.h"
#include "ej_limits.h"

#include "super-serve.h"
#include "super_proto.h"
#include "super_html.h"
#include "ejudge_cfg.h"
#include "contests.h"
#include "mischtml.h"
#include "xml_utils.h"
#include "serve_state.h"
#include "misctext.h"
#include "prepare.h"
#include "prepare_dflt.h"
#include "fileutl.h"

#include "reuse_xalloc.h"
#include "reuse_osdeps.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

#define SAVED_TEST_PREFIX "s_"
#define TEMP_TEST_PREFIX "t_"

#define ARMOR(s)  html_armor_buf(&ab, (s))
#define FAIL(c) do { retval = -(c); goto cleanup; } while (0)

static unsigned char *
ss_url_unescaped(
        unsigned char *buf,
        size_t size,
        const struct super_http_request_info *phr,
        int action,
        int op,
        const char *format,
        ...)
{
  unsigned char fbuf[1024];
  unsigned char abuf[64];
  unsigned char obuf[64];
  const unsigned char *sep = "";
  va_list args;

  fbuf[0] = 0;
  if (format && *format) {
    va_start(args, format);
    vsnprintf(fbuf, sizeof(fbuf), format, args);
    va_end(args);
  }
  if (fbuf[0]) sep = "&";

  abuf[0] = 0;
  if (action > 0) snprintf(abuf, sizeof(abuf), "&action=%d", action);
  obuf[0] = 0;
  if (op > 0) snprintf(obuf, sizeof(obuf), "&op=%d", op);

  snprintf(buf, size, "%s?SID=%016llx%s%s%s%s", phr->self_url,
           phr->session_id, abuf, obuf, sep, fbuf);
  return buf;
}

static void
ss_redirect_2(
        FILE *fout,
        struct super_http_request_info *phr,
        int new_op,
        int contest_id,
        int prob_id,
        int variant,
        int test_num)
{
  unsigned char url[1024];
  char *o_str = 0;
  size_t o_len = 0;
  FILE *o_out = 0;

  o_out = open_memstream(&o_str, &o_len);
  if (contest_id > 0) {
    fprintf(o_out, "&contest_id=%d", contest_id);
  }
  if (prob_id > 0) {
    fprintf(o_out, "&prob_id=%d", prob_id);
  }
  if (variant > 0) {
    fprintf(o_out, "&variant=%d", variant);
  }
  if (test_num > 0) {
    fprintf(o_out, "&test_num=%d", test_num);
  }
  fclose(o_out); o_out = 0;

  if (o_str && *o_str) {
    ss_url_unescaped(url, sizeof(url), phr, SSERV_CMD_HTTP_REQUEST, new_op, "%s", o_str);
  } else {
    ss_url_unescaped(url, sizeof(url), phr, SSERV_CMD_HTTP_REQUEST, new_op, 0);
  }

  xfree(o_str); o_str = 0; o_len = 0;

  fprintf(fout, "Content-Type: text/html; charset=%s\nCache-Control: no-cache\nPragma: no-cache\nLocation: %s\n\n", EJUDGE_CHARSET, url);
}

void
super_html_7_force_link()
{
}

static int
get_full_caps(const struct super_http_request_info *phr, const struct contest_desc *cnts, opcap_t *pcap)
{
  opcap_t caps1 = 0, caps2 = 0;

  opcaps_find(&phr->config->capabilities, phr->login, &caps1);
  opcaps_find(&cnts->capabilities, phr->login, &caps2);
  *pcap = caps1 | caps2;
  return 0;
}



static int
check_other_editors(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr,
        int contest_id,
        const struct contest_desc *cnts)
{
  unsigned char buf[1024];
  unsigned char hbuf[1024];
  const unsigned char *cl = " class=\"b0\"";
  time_t current_time = time(0);
  serve_state_t cs = NULL;
  struct stat stb;

  // check if this contest is already edited by anybody else
  const struct sid_state *other_session = super_serve_sid_state_get_cnts_editor(contest_id);

  if (other_session == phr->ss) {
    snprintf(buf, sizeof(buf), "serve-control: %s, the contest is being edited by you",
             phr->html_name);
    ss_write_html_header(out_f, phr, buf, 0, NULL);
    fprintf(out_f, "<h1>%s</h1>\n", buf);

    fprintf(out_f, "<ul>");
    fprintf(out_f, "<li>%s%s</a></li>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                          NULL, NULL),
            "Main page");
    fprintf(out_f, "</ul>\n");

    fprintf(out_f, "<p>To edit the tests you should finish editing the contest settings.</p>\n");

    ss_write_html_footer(out_f);
    return 0;
  }

  if (other_session) {
    snprintf(buf, sizeof(buf), "serve-control: %s, the contest is being edited by someone else",
             phr->html_name);
    ss_write_html_header(out_f, phr, buf, 0, NULL);
    fprintf(out_f, "<h1>%s</h1>\n", buf);

    fprintf(out_f, "<ul>");
    fprintf(out_f, "<li>%s%s</a></li>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                          NULL, NULL),
            "Main page");
    fprintf(out_f, "</ul>\n");

    fprintf(out_f, "<p>This contest is being edited by another user or in another session.</p>");
    fprintf(out_f, "<table%s><tr><td%s>%s</td><td%s>%016llx</td></tr>",
            cl, cl, "Session", cl, other_session->sid);
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr>",
            cl, "IP address", cl, xml_unparse_ip(other_session->remote_addr));
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr></table>\n",
            cl, "User login", cl, other_session->user_login);
    ss_write_html_footer(out_f);
    return 0;
  }

  other_session = super_serve_sid_state_get_test_editor(contest_id);
  if (other_session && other_session != phr->ss) {
    snprintf(buf, sizeof(buf), "serve-control: %s, the tests are being edited by someone else",
             phr->html_name);
    ss_write_html_header(out_f, phr, buf, 0, NULL);
    fprintf(out_f, "<h1>%s</h1>\n", buf);

    fprintf(out_f, "<ul>");
    fprintf(out_f, "<li>%s%s</a></li>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                          NULL, NULL),
            "Main page");
    fprintf(out_f, "</ul>\n");

    fprintf(out_f, "<p>This tests are being edited by another user or in another session.</p>");
    fprintf(out_f, "<table%s><tr><td%s>%s</td><td%s>%016llx</td></tr>",
            cl, cl, "Session", cl, other_session->sid);
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr>",
            cl, "IP address", cl, xml_unparse_ip(other_session->remote_addr));
    fprintf(out_f, "<tr><td%s>%s</td><td%s>%s</td></tr></table>\n",
            cl, "User login", cl, other_session->user_login);
    ss_write_html_footer(out_f);
    return 0;
  }

  if ((cs = phr->ss->te_state) && cs->last_timestamp > 0 && cs->last_check_time + 10 >= current_time) {
    return 1;
  }

  if (cs && cs->last_timestamp > 0) {
    if (!cs->config_path) goto invalid_serve_cfg;
    if (stat(cs->config_path, &stb) < 0) goto invalid_serve_cfg;
    if (!S_ISREG(stb.st_mode)) goto invalid_serve_cfg;
    if (stb.st_mtime == cs->last_timestamp) {
      cs->last_check_time = current_time;
      return 1;
    }
  }

  phr->ss->te_state = serve_state_destroy(cs, cnts, NULL);

  if (serve_state_load_contest_config(phr->config, contest_id, cnts, &phr->ss->te_state) < 0)
    goto invalid_serve_cfg;

  cs = phr->ss->te_state;
  if (!cs) goto invalid_serve_cfg;
  if (!cs->config_path) goto invalid_serve_cfg;
  if (stat(cs->config_path, &stb) < 0) goto invalid_serve_cfg;
  if (!S_ISREG(stb.st_mode)) goto invalid_serve_cfg;
  cs->last_timestamp = stb.st_mtime;
  cs->last_check_time = current_time;

  return 1;

invalid_serve_cfg:
  phr->ss->te_state = serve_state_destroy(cs, cnts, NULL);
  return -S_ERR_INV_SERVE_CONFIG_PATH;
}

int
super_serve_op_TESTS_MAIN_PAGE(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr)
{
  int retval = 0;
  int contest_id = 0;
  const struct contest_desc *cnts = NULL;
  opcap_t caps = 0;
  unsigned char buf[1024];
  unsigned char hbuf[1024];
  path_t adv_path;
  struct html_armor_buffer ab = HTML_ARMOR_INITIALIZER;
  const unsigned char *cl, *s;
  int prob_id, variant;
  serve_state_t cs;
  const struct section_problem_data *prob;
  int need_variant = 0;
  int need_statement = 0;
  int need_style_checker = 0;
  int need_valuer = 0;
  int need_interactor = 0;
  int need_test_checker = 0;
  int need_makefile = 0;
  int need_header = 0;
  int need_footer = 0;
  int variant_num = 0;

  FILE *prb_f = NULL;
  char *prb_t = NULL;
  size_t prb_z = 0;

  ss_cgi_param_int_opt(phr, "contest_id", &contest_id, 0);
  if (contest_id <= 0) FAIL(S_ERR_INV_CONTEST);
  if (contests_get(contest_id, &cnts) < 0 || !cnts) FAIL(S_ERR_INV_CONTEST);

  if (phr->priv_level < PRIV_LEVEL_JUDGE) FAIL(S_ERR_PERM_DENIED);
  get_full_caps(phr, cnts, &caps);
  if (opcaps_check(caps, OPCAP_CONTROL_CONTEST) < 0) FAIL(S_ERR_PERM_DENIED);

  retval = check_other_editors(log_f, out_f, phr, contest_id, cnts);
  if (retval <= 0) goto cleanup;
  retval = 0;
  cs = phr->ss->te_state;

  for (prob_id = 1; prob_id <= cs->max_prob; ++prob_id) {
    if (!(prob = cs->probs[prob_id])) continue;
    if (prob->variant_num > 0) need_variant = 1;
    if (prob->xml_file && prob->xml_file[0]) need_statement = 1;
    if (prob->style_checker_cmd && prob->style_checker_cmd[0]) need_style_checker = 1;
    if (prob->valuer_cmd && prob->valuer_cmd[0]) need_valuer = 1;
    if (prob->interactor_cmd && prob->interactor_cmd[0]) need_interactor = 1;
    if (prob->test_checker_cmd && prob->test_checker_cmd[0]) need_test_checker = 1;
    if (prob->source_header && prob->source_header[0]) need_header = 1;
    if (prob->source_footer && prob->source_footer[0]) need_footer = 1;
  }
  if (cs->global->advanced_layout > 0) need_makefile = 1;

  snprintf(buf, sizeof(buf), "serve-control: %s, contest %d (%s)",
           phr->html_name, contest_id, ARMOR(cnts->name));
  ss_write_html_header(out_f, phr, buf, 0, NULL);
  fprintf(out_f, "<h1>%s</h1>\n", buf);

  fprintf(out_f, "<ul>");
  fprintf(out_f, "<li>%s%s</a></li>",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL, NULL),
          "Main page");
  fprintf(out_f, "</ul>\n");

  fprintf(out_f, "<h2>%s</h2>\n", "Problems");

  cl = " class=\"b1\"";
  fprintf(out_f, "<table%s>", cl);
  fprintf(out_f,
          "<tr>"
          "<th%s>%s</th>"
          "<th%s>%s</th>"
          "<th%s>%s</th>"
          "<th%s>%s</th>"
          "<th%s>%s</th>"
          "<th%s>%s</th>",
          cl, "Prob. ID", cl, "Short name", cl, "Long name", cl, "Int. name", cl, "Type", cl, "Config");
  if (need_variant > 0) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Variant");
  }
  if (need_statement > 0) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Statement");
  }
  if (need_header > 0) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Source header");
  }
  if (need_footer > 0) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Source footer");
  }
  if (need_style_checker > 0) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Style checker");
  }
  fprintf(out_f, "<th%s>%s</th>", cl, "Tests");
  fprintf(out_f, "<th%s>%s</th>", cl, "Checker");
  if (need_valuer) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Valuer");
  }
  if (need_interactor) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Interactor");
  }
  if (need_test_checker) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Test checker");
  }
  if (need_makefile) {
    fprintf(out_f, "<th%s>%s</th>", cl, "Makefile");
  }
  fprintf(out_f, "</tr>\n");
  for (prob_id = 1; prob_id <= cs->max_prob; ++prob_id) {
    if (!(prob = cs->probs[prob_id])) continue;

    variant_num = prob->variant_num;
    if (variant_num < 0) variant_num = 0;
    variant = 0;
    if (prob->variant_num > 0) variant = 1;
    do {
      fprintf(out_f, "<tr>");
      if (variant <= 1) {
        prb_f = open_memstream(&prb_t, &prb_z);
        prepare_unparse_actual_prob(prb_f, prob, cs->global, 0);
        fclose(prb_f); prb_f = NULL;

        fprintf(out_f, "<td%s>%d</td>", cl, prob_id);
        fprintf(out_f, "<td%s>%s</td>", cl, ARMOR(prob->short_name));
        fprintf(out_f, "<td%s>%s</td>", cl, ARMOR(prob->long_name));
        s = prob->short_name;
        if (prob->internal_name[0]) {
          s = prob->internal_name;
        }
        fprintf(out_f, "<td%s>%s</td>", cl, ARMOR(s));
        fprintf(out_f, "<td%s>%s</td>", cl, problem_unparse_type(prob->type));
        fprintf(out_f, "<td%s><font size=\"-1\"><pre>%s</pre></font></td>", cl, ARMOR(prb_t));
        free(prb_t); prb_t = NULL; prb_z = 0;
      } else {
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
      }

      // variant
      if (need_variant) {
        if (prob->variant_num <= 0) {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        } else {
          fprintf(out_f, "<td%s>%d</td>", cl, variant);
        }
      }
      // statement
      if (need_statement) {
        if (prob->xml_file && prob->xml_file[0]) {
          if (cs->global->advanced_layout > 0) {
            get_advanced_layout_path(adv_path, sizeof(adv_path), cs->global,
                                     prob, prob->xml_file, variant);
          } else if (variant > 0) {
            prepare_insert_variant_num(adv_path, sizeof(adv_path), prob->xml_file, variant);
          } else {
            snprintf(adv_path, sizeof(adv_path), "%s", prob->xml_file);
          }
          fprintf(out_f, "<td title=\"%s\"%s>%s%s</a></td>",
                  ARMOR(adv_path), cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_STATEMENT_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }

      // source header
      if (need_header) {
        if (prob->source_header && prob->source_header[0]) {
          fprintf(out_f, "<td title=\"%s\"%s>%s%s</a></td>",
                  ARMOR(prob->source_header), cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_SOURCE_HEADER_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }

      // source footer
      if (need_footer) {
        if (prob->source_footer && prob->source_footer[0]) {
          fprintf(out_f, "<td title=\"%s\"%s>%s%s</a></td>",
                  ARMOR(prob->source_footer), cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_SOURCE_FOOTER_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }

      // style checker
      if (need_style_checker) {
        if (prob->style_checker_cmd && prob->style_checker_cmd[0]) {
          fprintf(out_f, "<td%s>%s%s</a></td>",
                  cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_STYLE_CHECKER_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }

      // tests
      if (cs->global->advanced_layout > 0) {
        get_advanced_layout_path(adv_path, sizeof(adv_path), cs->global,
                                 prob, DFLT_P_TEST_DIR, variant);
      } else if (variant > 0) {
        snprintf(adv_path, sizeof(adv_path), "%s-%d", prob->test_dir, variant);
      } else {
        snprintf(adv_path, sizeof(adv_path), "%s", prob->test_dir);
      }
      fprintf(out_f, "<td title=\"%s\"%s>%s%s</a></td>",
              ARMOR(adv_path), cl, 
              html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                            NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                            SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_TESTS_VIEW_PAGE,
                            contest_id, variant, prob_id),
              "View");

      // checker
      if (prob->standard_checker && prob->standard_checker[0]) {
        s = super_html_get_standard_checker_description(prob->standard_checker);
        if (!s) s = "???";
        fprintf(out_f, "<td title=\"%s\"%s>", ARMOR(s), cl);
        fprintf(out_f, "<tt>%s</tt></td>", ARMOR(prob->standard_checker));
      } else if (prob->check_cmd && prob->check_cmd[0]) {
        fprintf(out_f, "<td%s>%s%s</a></td>",
                cl, 
                html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                              NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                              SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_CHECKER_EDIT_PAGE,
                              contest_id, variant, prob_id),
                "Edit");
      } else {
        fprintf(out_f, "<td%s>&nbsp;</td>", cl);
      }
      // valuer
      if (need_valuer) {
        if (prob->valuer_cmd && prob->valuer_cmd[0]) {
          fprintf(out_f, "<td%s>%s%s</a></td>",
                  cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_VALUER_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }
      // interactor
      if (need_interactor) {
        if (prob->interactor_cmd && prob->interactor_cmd[0]) {
          fprintf(out_f, "<td%s>%s%s</a></td>",
                  cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_INTERACTOR_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }
      // test checker
      if (need_test_checker) {
        if (prob->test_checker_cmd && prob->test_checker_cmd[0]) {
          fprintf(out_f, "<td%s>%s%s</a></td>",
                  cl, 
                  html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url,
                                NULL, "action=%d&amp;op=%d&amp;contest_id=%d&amp;variant=%d&amp;prob_id=%d",
                                SSERV_CMD_HTTP_REQUEST, SSERV_OP_TESTS_TEST_CHECKER_EDIT_PAGE,
                                contest_id, variant, prob_id),
                  "Edit");
        } else {
          fprintf(out_f, "<td%s>&nbsp;</td>", cl);
        }
      }
      if (need_makefile) {
      }
      fprintf(out_f, "</tr>");
    } while (++variant <= prob->variant_num);
  }
  fprintf(out_f, "</table>\n");

  ss_write_html_footer(out_f);

cleanup:
  html_armor_free(&ab);
  return retval;
}

// file classification
enum
{
  TESTS_TEST_FILE = 1,
  TESTS_CORR_FILE = 2,
  TESTS_INFO_FILE = 3,
  TESTS_SAVED_TEST_FILE = 5,
  TESTS_SAVED_CORR_FILE = 6,
  TESTS_SAVED_INFO_FILE = 7,
  TESTS_README_FILE = 9,
};

struct test_file_info
{
  unsigned char *name;
  int mode;
  int user;
  int group;
  long long size;
  time_t mtime;
  int use;
  int use_idx;
};
struct test_info
{
  int test_idx;
  int corr_idx;
  int info_idx;
  int tgz_idx;
};
struct test_dir_info
{
  int u, a;
  struct test_file_info *v;
  int test_ref_count;
  struct test_info *test_refs;
  int saved_ref_count;
  struct test_info *saved_refs;
  int readme_idx;
};

static void
test_dir_info_free(struct test_dir_info *ptd)
{
  int i;

  if (!ptd) return;

  xfree(ptd->test_refs);
  xfree(ptd->saved_refs);
  for (i = 0; i < ptd->u; ++i) {
    xfree(ptd->v[i].name);
  }
  xfree(ptd->v);
  memset(ptd, 0, sizeof(*ptd));
}

static int
files_sort_func(const void *vp1, const void *vp2)
{
  const struct test_file_info *i1 = (const struct test_file_info *) vp1;
  const struct test_file_info *i2 = (const struct test_file_info *) vp2;
  return strcmp(i1->name, i2->name);
}

static int
scan_test_directory(
        FILE *log_f,
        struct test_dir_info *files,
        const struct contest_desc *cnts,
        const unsigned char *test_dir,
        const unsigned char *test_pat,
        const unsigned char *corr_pat,
        const unsigned char *info_pat)
{
  struct stat stb;
  DIR *d = NULL;
  struct dirent *dd = NULL;
  int retval = 0;
  unsigned char fullpath[PATH_MAX];
  unsigned char name[PATH_MAX];
  unsigned char saved_pat[PATH_MAX];
  int new_a = 0;
  struct test_file_info *new_v = NULL;
  int test_count, corr_count, info_count, common_count, low, high, mid, v, i;

  if (stat(test_dir, &stb) < 0) {
    if (os_MakeDirPath2(test_dir, cnts->dir_mode, cnts->dir_group) < 0) {
      fprintf(log_f, "failed to created test directory '%s'\n", test_dir);
      FAIL(S_ERR_INV_CNTS_SETTINGS);
    }
  }
  if (stat(test_dir, &stb) < 0) {
    fprintf(log_f, "test directory does not exist and cannot be created\n");
    FAIL(S_ERR_INV_CNTS_SETTINGS);
  }
  if (!S_ISDIR(stb.st_mode)) {
    fprintf(log_f, "test directory is not a directory\n");
    FAIL(S_ERR_INV_CNTS_SETTINGS);
  }
  if (access(test_dir, R_OK | X_OK) < 0) {
    fprintf(log_f, "test directory is not readable\n");
    FAIL(S_ERR_INV_CNTS_SETTINGS);
  }

  if (!(d = opendir(test_dir))) {
    fprintf(log_f, "test directory cannot be opened\n");
    FAIL(S_ERR_INV_CNTS_SETTINGS);
  }
  while ((dd = readdir(d))) {
    if (!strcmp(dd->d_name, ".") || !strcmp(dd->d_name, "..")) continue;
    snprintf(fullpath, sizeof(fullpath), "%s/%s", test_dir, dd->d_name);
    if (stat(fullpath, &stb) < 0) continue;
    if (access(fullpath, R_OK) < 0) continue;

    if (files->u >= files->a) {
      if (!(new_a = files->a * 2)) new_a= 32;
      XCALLOC(new_v, new_a);
      if (files->u > 0) {
        memcpy(new_v, files->v, files->u * sizeof(new_v[0]));
      }
      xfree(files->v);
      files->v = new_v;
      files->a = new_a;
    }
    files->v[files->u].name = xstrdup(dd->d_name);
    files->v[files->u].mode = stb.st_mode & 07777;
    files->v[files->u].user = stb.st_uid;
    files->v[files->u].group = stb.st_gid;
    files->v[files->u].size = stb.st_size;
    files->v[files->u].mtime = stb.st_mtime;
    ++files->u;
  }
  closedir(d); d = NULL;

  qsort(files->v, files->u, sizeof(files->v[0]), files_sort_func);

  // detect how many test files
  test_count = 0;
  do {
    ++test_count;
    snprintf(name, sizeof(name), test_pat, test_count);
    low = 0; high = files->u;
    while (low < high) {
      mid = (low + high) / 2;
      if (!(v = strcmp(files->v[mid].name, name))) break;
      if (v < 0) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
  } while (low < high);
  --test_count;

  // detect how many answer files
  corr_count = 0;
  if (corr_pat && corr_pat[0]) {
    do {
      ++corr_count;
      snprintf(name, sizeof(name), corr_pat, corr_count);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
    } while (low < high);
    --corr_count;
  }

  // detect how many info files
  info_count = 0;
  if (info_pat && info_pat[0]) {
    do {
      ++info_count;
      snprintf(name, sizeof(name), info_pat, info_count);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
    } while (low < high);
    --info_count;
  }

  common_count = test_count;
  if (corr_count > common_count) common_count = corr_count;
  if (info_count > common_count) common_count = info_count;
  if (common_count > 0) {
    files->test_ref_count = common_count;
    XCALLOC(files->test_refs, common_count);
    for (i = 0; i < common_count; ++i) {
      files->test_refs[i].test_idx = -1;
      files->test_refs[i].corr_idx = -1;
      files->test_refs[i].info_idx = -1;
      files->test_refs[i].tgz_idx = -1;
    }
  }

  // scan for saved files
  test_count = 0;
  snprintf(saved_pat, sizeof(saved_pat), "%s%s", SAVED_TEST_PREFIX, test_pat);
  do {
    ++test_count;
    snprintf(name, sizeof(name), saved_pat, test_count);
    low = 0; high = files->u;
    while (low < high) {
      mid = (low + high) / 2;
      if (!(v = strcmp(files->v[mid].name, name))) break;
      if (v < 0) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }    
  } while (low < high);
  --test_count;

  corr_count = 0;
  if (corr_pat && corr_pat[0]) {
    snprintf(saved_pat, sizeof(saved_pat), "%s%s", SAVED_TEST_PREFIX, corr_pat);
    do {
      ++corr_count;
      snprintf(name, sizeof(name), saved_pat, corr_count);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
    } while (low < high);
    --corr_count;
  }

  info_count = 0;
  if (info_pat && info_pat[0]) {
    snprintf(saved_pat, sizeof(saved_pat), "%s%s", SAVED_TEST_PREFIX, info_pat);
    do {
      ++info_count;
      snprintf(name, sizeof(name), saved_pat, info_count);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
    } while (low < high);
    --info_count;
  }

  common_count = test_count;
  if (corr_count > common_count) common_count = corr_count;
  if (info_count > common_count) common_count = info_count;
  if (common_count > 0) {
    files->saved_ref_count = common_count;
    XCALLOC(files->saved_refs, common_count);
    for (i = 0; i < common_count; ++i) {
      files->saved_refs[i].test_idx = -1;
      files->saved_refs[i].corr_idx = -1;
      files->saved_refs[i].info_idx = -1;
      files->saved_refs[i].tgz_idx = -1;
    }
  }

  // sort out the files
  for (i = 1; i <= files->test_ref_count; ++i) {
    snprintf(name, sizeof(name), test_pat, i);
    low = 0; high = files->u;
    while (low < high) {
      mid = (low + high) / 2;
      if (!(v = strcmp(files->v[mid].name, name))) break;
      if (v < 0) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    if (low < high) {
      files->test_refs[i - 1].test_idx = mid;
      files->v[mid].use = TESTS_TEST_FILE;
      files->v[mid].use_idx = i;
    }
    if (corr_pat && corr_pat[0]) {
      snprintf(name, sizeof(name), corr_pat, i);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
      if (low < high) {
        files->test_refs[i - 1].corr_idx = mid;
        files->v[mid].use = TESTS_CORR_FILE;
        files->v[mid].use_idx = i;
      }
    }
    if (info_pat && info_pat[0]) {
      snprintf(name, sizeof(name), info_pat, i);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
      if (low < high) {
        files->test_refs[i - 1].info_idx = mid;
        files->v[mid].use = TESTS_INFO_FILE;
        files->v[mid].use_idx = i;
      }
    }
  }

  for (i = 1; i <= files->saved_ref_count; ++i) {
    snprintf(saved_pat, sizeof(saved_pat), "s_%s", test_pat);
    snprintf(name, sizeof(name), saved_pat, i);
    low = 0; high = files->u;
    while (low < high) {
      mid = (low + high) / 2;
      if (!(v = strcmp(files->v[mid].name, name))) break;
      if (v < 0) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    if (low < high) {
      files->saved_refs[i - 1].test_idx = mid;
      files->v[mid].use = TESTS_SAVED_TEST_FILE;
      files->v[mid].use_idx = i;
    }
    if (corr_pat && corr_pat[0]) {
      snprintf(saved_pat, sizeof(saved_pat), "s_%s", corr_pat);
      snprintf(name, sizeof(name), saved_pat, i);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
      if (low < high) {
        files->saved_refs[i - 1].corr_idx = mid;
        files->v[mid].use = TESTS_SAVED_CORR_FILE;
        files->v[mid].use_idx = i;
      }
    }
    if (info_pat && info_pat[0]) {
      snprintf(saved_pat, sizeof(saved_pat), "s_%s", info_pat);
      snprintf(name, sizeof(name), saved_pat, i);
      low = 0; high = files->u;
      while (low < high) {
        mid = (low + high) / 2;
        if (!(v = strcmp(files->v[mid].name, name))) break;
        if (v < 0) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
      if (low < high) {
        files->saved_refs[i - 1].info_idx = mid;
        files->v[mid].use = TESTS_SAVED_INFO_FILE;
        files->v[mid].use_idx = i;
      }
    }
  }

  // scan for README
  files->readme_idx = -1;
  snprintf(name, sizeof(name), "README");
  low = 0; high = files->u;
  while (low < high) {
    mid = (low + high) / 2;
    if (!(v = strcmp(files->v[mid].name, name))) break;
    if (v < 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  if (low < high) {
    files->readme_idx = mid;
    files->v[mid].use = TESTS_README_FILE;
  }

cleanup:
  if (d) closedir(d);
  return retval;
}

static int
is_text_file(const unsigned char *txt, size_t size)
{
  size_t i;

  if (!txt) return 0;
  for (i = 0; i < size; ++i) {
    if (txt[i] == 0177) return 0;
    if (txt[i] < ' ' && !isspace(txt[i])) return 0;
  }
  return 1;
}

static void
output_text_file(FILE *out_f, const unsigned char *txt, size_t size)
{
  size_t i;

  fprintf(out_f, "<br/><hr/><pre>");
  for (i = 0; i < size; ++i) {
    switch (txt[i]) {
    case '<':
      fprintf(out_f, "&lt;");
      break;
    case '>':
      fprintf(out_f, "&gt;");
      break;
    case '&':
      fprintf(out_f, "&amp;");
      break;
    case '"':
      fprintf(out_f, "&quot;");
      break;
    case '\t':
      fprintf(out_f, "&rarr;");
      break;
    case '\r':
      fprintf(out_f, "&crarr;");
      break;
    case '\n':
      fprintf(out_f, "&para;\n");
      break;
    default:
      putc(txt[i], out_f);
      break;
    }
  }
  fprintf(out_f, "</pre>");
}

static void
report_file(
        FILE *out_f,
        const unsigned char *dir_path,
        const struct test_dir_info *files,
        int index,
        const unsigned char *cl)
{
  char *file_t = 0;
  size_t file_z = 0;

  if (index < 0 || index >= files->u) {
    fprintf(out_f, "<td%s valign=\"top\"><i>%s</i></td>", cl, "nonexisting");
    return;
  }
  fprintf(out_f, "<td%s valign=\"top\">", cl);
  fprintf(out_f, "<i>%s</i><br/>", xml_unparse_date(files->v[index].mtime));
  fprintf(out_f, "<i>%lld</i>", files->v[index].size);
  if (files->v[index].size > 0 || files->v[index].size <= 256) {
    if (generic_read_file(&file_t, 0, &file_z, 0, dir_path, files->v[index].name, "") < 0) {
      fprintf(out_f, "<br/><i>%s</i>", "read error");
    } else if (!is_text_file(file_t, file_z)) {
      xfree(file_t); file_t = 0; file_z = 0;
      fprintf(out_f, "<br/><i>%s</i>", "binary file");
    } else {
      output_text_file(out_f, file_t, file_z);
      xfree(file_t); file_t = 0; file_z = 0;
    }
  }
  fprintf(out_f, "</td>");
}

static int
prepare_test_file_names(
        FILE *log_f,
        struct super_http_request_info *phr,
        const struct contest_desc *cnts,
        const struct section_global_data *global,
        const struct section_problem_data *prob,
        int variant,
        const unsigned char *pat_prefix,
        int buf_size,
        unsigned char *test_dir,
        unsigned char *test_pat,
        unsigned char *corr_pat,
        unsigned char *info_pat)
{
  int retval;
  unsigned char corr_dir[PATH_MAX];
  unsigned char info_dir[PATH_MAX];
  unsigned char name1[PATH_MAX];
  unsigned char name2[PATH_MAX];

  if (pat_prefix == NULL) pat_prefix = "";

  test_dir[0] = 0;
  test_pat[0] = 0;
  corr_pat[0] = 0;
  info_pat[0] = 0;
  corr_dir[0] = 0;
  info_dir[0] = 0;

  if (global->advanced_layout > 0) {
    get_advanced_layout_path(test_dir, buf_size, global, prob, DFLT_P_TEST_DIR, variant);
  } else if (variant > 0) {
    snprintf(test_dir, buf_size, "%s-%d", prob->test_dir, variant);
  } else {
    snprintf(test_dir, buf_size, "%s", prob->test_dir);
  }
  if (prob->test_pat[0] >= ' ') {
    snprintf(test_pat, buf_size, "%s%s", pat_prefix, prob->test_pat);
  } else if (prob->test_sfx[0] >= ' ') {
    snprintf(test_pat, buf_size, "%s%%03d%s", pat_prefix, prob->test_sfx);
  } else {
    snprintf(test_pat, buf_size, "%s%%03d%s", pat_prefix, ".dat");
  }
  snprintf(name1, sizeof(name1), test_pat, 1);
  snprintf(name2, sizeof(name2), test_pat, 2);
  if (!strcmp(name1, name2)) {
    fprintf(log_f, "invalid test files pattern\n");
    FAIL(S_ERR_UNSUPPORTED_SETTINGS);
  }

  corr_dir[0] = 0;
  corr_pat[0] = 0;
  if (prob->use_corr > 0) {
    if (global->advanced_layout > 0) {
      get_advanced_layout_path(corr_dir, sizeof(corr_dir), global, prob, DFLT_P_CORR_DIR, variant);
    } else if (variant > 0) {
      snprintf(corr_dir, sizeof(corr_dir), "%s-%d", prob->corr_dir, variant);
    } else {
      snprintf(corr_dir, sizeof(corr_dir), "%s", prob->corr_dir);
    }
    if (strcmp(corr_dir, test_dir) != 0) {
      fprintf(log_f, "corr_dir and test_dir cannot be different\n");
      FAIL(S_ERR_UNSUPPORTED_SETTINGS);
    }
    if (prob->corr_pat[0] >= ' ' ) {
      snprintf(corr_pat, buf_size, "%s%s", pat_prefix, prob->corr_pat);
    } else if (prob->corr_sfx[0] >= ' ') {
      snprintf(corr_pat, buf_size, "%s%%03d%s", pat_prefix, prob->corr_sfx);
    } else {
      snprintf(corr_pat, buf_size, "%s%%03d%s", pat_prefix, ".ans");
    }
    snprintf(name1, sizeof(name1), corr_pat, 1);
    snprintf(name2, sizeof(name2), corr_pat, 2);
    if (!strcmp(name1, name2)) {
      fprintf(log_f, "invalid correct files pattern\n");
      FAIL(S_ERR_UNSUPPORTED_SETTINGS);
    }
  }

  info_dir[0] = 0;
  info_pat[0] = 0;
  if (prob->use_info > 0) {
    if (global->advanced_layout > 0) {
      get_advanced_layout_path(info_dir, sizeof(info_dir), global, prob, DFLT_P_INFO_DIR, variant);
    } else if (variant > 0) {
      snprintf(info_dir, sizeof(info_dir), "%s-%d", prob->info_dir, variant);
    } else {
      snprintf(info_dir, sizeof(info_dir), "%s", prob->info_dir);
    }
    if (strcmp(info_dir, test_dir) != 0) {
      fprintf(log_f, "info_dir and test_dir cannot be different\n");
      FAIL(S_ERR_UNSUPPORTED_SETTINGS);
    }
    if (prob->info_pat[0] >= ' ' ) {
      snprintf(info_pat, buf_size, "%s%s", pat_prefix, prob->info_pat);
    } else if (prob->corr_sfx[0] >= ' ') {
      snprintf(info_pat, buf_size, "%s%%03d%s", pat_prefix, prob->info_sfx);
    } else {
      snprintf(info_pat, buf_size, "%s%%03d%s", pat_prefix, ".inf");
    }
    snprintf(name1, sizeof(name1), info_pat, 1);
    snprintf(name2, sizeof(name2), info_pat, 2);
    if (!strcmp(name1, name2)) {
      fprintf(log_f, "invalid info files pattern\n");
      FAIL(S_ERR_UNSUPPORTED_SETTINGS);
    }
  }

cleanup:
  return retval;
}

int
super_serve_op_TESTS_TESTS_VIEW_PAGE(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr)
{
  int retval = 0;
  int contest_id = 0;
  const struct contest_desc *cnts = NULL;
  opcap_t caps = 0LL;
  serve_state_t cs = NULL;
  int prob_id = 0, variant = 0;
  const struct section_global_data *global = NULL;
  const struct section_problem_data *prob = NULL;
  unsigned char test_dir[PATH_MAX];
  unsigned char test_pat[PATH_MAX];
  unsigned char corr_pat[PATH_MAX];
  unsigned char info_pat[PATH_MAX];
  struct test_dir_info td_info;
  int i;
  unsigned char buf[1024], hbuf[1024];
  struct html_armor_buffer ab = HTML_ARMOR_INITIALIZER;
  const unsigned char *cl = "";

  memset(&td_info, 0, sizeof(td_info));

  ss_cgi_param_int_opt(phr, "contest_id", &contest_id, 0);
  if (contest_id <= 0) FAIL(S_ERR_INV_CONTEST);
  if (contests_get(contest_id, &cnts) < 0 || !cnts) FAIL(S_ERR_INV_CONTEST);

  if (phr->priv_level < PRIV_LEVEL_JUDGE) FAIL(S_ERR_PERM_DENIED);
  get_full_caps(phr, cnts, &caps);
  if (opcaps_check(caps, OPCAP_CONTROL_CONTEST) < 0) FAIL(S_ERR_PERM_DENIED);

  retval = check_other_editors(log_f, out_f, phr, contest_id, cnts);
  if (retval <= 0) goto cleanup;
  retval = 0;
  cs = phr->ss->te_state;
  global = cs->global;

  ss_cgi_param_int_opt(phr, "prob_id", &prob_id, 0);
  if (prob_id <= 0 || prob_id > cs->max_prob) FAIL(S_ERR_INV_PROB_ID);
  if (!(prob = cs->probs[prob_id])) FAIL(S_ERR_INV_PROB_ID);

  variant = -1;
  if (prob->variant_num > 0) {
    ss_cgi_param_int_opt(phr, "variant", &variant, 0);
    if (variant <= 0 || variant > prob->variant_num) FAIL(S_ERR_INV_VARIANT);
  }

  retval = prepare_test_file_names(log_f, phr, cnts, global, prob, variant, NULL,
                                   sizeof(test_dir), test_dir, test_pat, corr_pat, info_pat);
  if (retval < 0) goto cleanup;
  retval = 0;

  retval = scan_test_directory(log_f, &td_info, cnts, test_dir, test_pat, corr_pat, info_pat);
  if (retval < 0) goto cleanup;

  snprintf(buf, sizeof(buf), "serve-control: %s, contest %d (%s), tests for problem %s",
           phr->html_name, contest_id, ARMOR(cnts->name), prob->short_name);
  ss_write_html_header(out_f, phr, buf, 0, NULL);
  fprintf(out_f, "<h1>%s</h1>\n", buf);

  fprintf(out_f, "<ul>");
  fprintf(out_f, "<li>%s%s</a></li>",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL, NULL),
          "Main page");
  fprintf(out_f, "<li>%s%s</a></li>\n",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                        "action=%d&op=%d&contest_id=%d", SSERV_CMD_HTTP_REQUEST,
                        SSERV_OP_TESTS_MAIN_PAGE, contest_id),
          "Problems page");
  fprintf(out_f, "</ul>\n");

  if (td_info.readme_idx >= 0) {
    fprintf(out_f, "<h2>%s</h2>\n", "README");

    cl = " class=\"b0\"";
    fprintf(out_f, "<table%s><tr><td%s>", cl, cl);
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_README_EDIT_PAGE, contest_id, prob_id),
            "Edit");
    fprintf(out_f, "</td><td%s>", cl);
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_README_DELETE, contest_id, prob_id),
            "Delete");
    fprintf(out_f, "</td></tr></table>\n");
  } else {
    fprintf(out_f, "<h2>%s</h2>\n", "README");

    cl = " class=\"b0\"";
    fprintf(out_f, "<table%s><tr><td%s>", cl, cl);
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_README_CREATE_PAGE, contest_id, prob_id),
            "Create");
    fprintf(out_f, "</td></tr></table>\n");
  }

  fprintf(out_f, "<h2>%s</h2>\n", "Active tests");

  cl = " class=\"b1\"";
  fprintf(out_f, "<table%s>", cl);
  for (i = 0; i < td_info.test_ref_count; ++i) {
    fprintf(out_f, "<tr>");
    fprintf(out_f, "<td%s>%d</td>", cl, i + 1);
    // test file info
    report_file(out_f, test_dir, &td_info, td_info.test_refs[i].test_idx, cl);
    if (corr_pat[0]) {
      report_file(out_f, test_dir, &td_info, td_info.test_refs[i].corr_idx, cl);
    }
    if (info_pat[0]) {
      report_file(out_f, test_dir, &td_info, td_info.test_refs[i].info_idx, cl);
    }
    fprintf(out_f, "<td%s>", cl);
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_TEST_MOVE_UP, contest_id, prob_id, i + 1),
            "Move up");
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_TEST_MOVE_DOWN, contest_id, prob_id, i + 1),
            "Move down");
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_TEST_MOVE_TO_SAVED, contest_id, prob_id, i + 1),
            "Move to saved");
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_TEST_INSERT_PAGE, contest_id, prob_id, i + 1),
            "Insert before");
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_TEST_EDIT_PAGE, contest_id, prob_id, i + 1),
            "Edit");
    fprintf(out_f, "&nbsp;%s[%s]</a>",
            html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                          "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                          SSERV_OP_TESTS_TEST_DELETE_PAGE, contest_id, prob_id, i + 1),
            "Delete");
    fprintf(out_f, "</td>");
    fprintf(out_f, "</tr>\n");
  }
  fprintf(out_f, "</table>\n");

  cl = " class=\"b0\"";
  fprintf(out_f, "<table%s><tr><td%s>", cl, cl);
  fprintf(out_f, "&nbsp;%s[%s]</a>",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                        "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                        SSERV_OP_TESTS_TEST_INSERT_PAGE, contest_id, prob_id, i + 1),
          "Add a new test after the last test");
  fprintf(out_f, "</td><td%s>", cl);
  fprintf(out_f, "&nbsp;%s[%s]</a>",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                        "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d", SSERV_CMD_HTTP_REQUEST,
                        SSERV_OP_TESTS_TEST_UPLOAD_ARCHIVE_1_PAGE, contest_id, prob_id),
          "Upload an archive of tests");
  fprintf(out_f, "</td></tr></table>\n");

  if (td_info.saved_ref_count > 0) {
    fprintf(out_f, "<h2>%s</h2>\n", "Saved tests");

    cl = " class=\"b1\"";
    fprintf(out_f, "<table%s>", cl);
    for (i = 0; i < td_info.saved_ref_count; ++i) {
      fprintf(out_f, "<tr>");
      fprintf(out_f, "<td%s>%d</td>", cl, i + 1);
      report_file(out_f, test_dir, &td_info, td_info.saved_refs[i].test_idx, cl);
      if (corr_pat[0]) {
        report_file(out_f, test_dir, &td_info, td_info.saved_refs[i].corr_idx, cl);
      }
      if (info_pat[0]) {
        report_file(out_f, test_dir, &td_info, td_info.saved_refs[i].info_idx, cl);
      }
      fprintf(out_f, "<td%s>", cl);
      fprintf(out_f, "&nbsp;%s[%s]</a>",
              html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                            "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                            SSERV_OP_TESTS_SAVED_MOVE_UP, contest_id, prob_id, i + 1),
              "Move up");
      fprintf(out_f, "&nbsp;%s[%s]</a>",
              html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                            "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                            SSERV_OP_TESTS_SAVED_MOVE_DOWN, contest_id, prob_id, i + 1),
              "Move down");
      fprintf(out_f, "&nbsp;%s[%s]</a>",
              html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                            "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                            SSERV_OP_TESTS_SAVED_MOVE_TO_TEST, contest_id, prob_id, i + 1),
              "Move to tests");
      fprintf(out_f, "&nbsp;%s[%s]</a>",
              html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                            "action=%d&amp;op=%d&amp;contest_id=%d&amp;prob_id=%d&amp;test_num=%d", SSERV_CMD_HTTP_REQUEST,
                            SSERV_OP_TESTS_SAVED_DELETE_PAGE, contest_id, prob_id, i + 1),
              "Delete");
      fprintf(out_f, "</td>");
      fprintf(out_f, "</tr>\n");
    }
    fprintf(out_f, "</table>\n");
  }

  ss_write_html_footer(out_f);

cleanup:
  test_dir_info_free(&td_info);
  html_armor_free(&ab);

  return retval;
}

static void
make_prefixed_path(
        unsigned char *path,
        size_t size,
        const unsigned char *dir,
        const unsigned char *prefix,
        const unsigned char *format,
        int value)
{
  unsigned char name[1024];

  if (!format || !*format) {
    memset(path, 0, size);
  } else {
    if (!prefix) prefix = "";
    snprintf(name, sizeof(name), format, value);
    snprintf(path, size, "%s/%s%s", dir, prefix, name);
  }
}

static int
logged_rename(
        FILE *log_f,
        const unsigned char *oldpath,
        const unsigned char *newpath)
{
  if (!*oldpath || !*newpath) return 0;

  if (rename(oldpath, newpath) < 0 && errno != ENOENT) {
    fprintf(log_f, "rename: %s->%s failed: %s\n", oldpath, newpath, os_ErrorMsg());
    return -1;
  }
  return 0;
}

static int
logged_unlink(
        FILE *log_f,
        const unsigned char *path)
{
  if (!*path) return 0;

  if (unlink(path) < 0 && errno != ENOENT) {
    fprintf(log_f, "unlink: %s failed: %s\n", path, os_ErrorMsg());
    return -1;
  }
  return 0;
}

static int
swap_files(
        FILE *log_f,
        const unsigned char *test_dir,
        const unsigned char *test_pat,
        const unsigned char *corr_pat,
        const unsigned char *info_pat,
        const unsigned char *src_prefix,
        const unsigned char *dst_prefix,
        const unsigned char *tmp_prefix,
        int src_num,
        int dst_num)
{
  int retval = 0, stage = 0;
  unsigned char test_src_path[PATH_MAX];
  unsigned char test_dst_path[PATH_MAX];
  unsigned char test_tmp_path[PATH_MAX];
  unsigned char corr_src_path[PATH_MAX];
  unsigned char corr_dst_path[PATH_MAX];
  unsigned char corr_tmp_path[PATH_MAX];
  unsigned char info_src_path[PATH_MAX];
  unsigned char info_dst_path[PATH_MAX];
  unsigned char info_tmp_path[PATH_MAX];

  make_prefixed_path(test_src_path, sizeof(test_src_path), test_dir, src_prefix, test_pat, src_num);
  make_prefixed_path(test_dst_path, sizeof(test_dst_path), test_dir, dst_prefix, test_pat, dst_num);
  make_prefixed_path(test_tmp_path, sizeof(test_tmp_path), test_dir, tmp_prefix, test_pat, dst_num);
  make_prefixed_path(corr_src_path, sizeof(corr_src_path), test_dir, src_prefix, corr_pat, src_num);
  make_prefixed_path(corr_dst_path, sizeof(corr_dst_path), test_dir, dst_prefix, corr_pat, dst_num);
  make_prefixed_path(corr_tmp_path, sizeof(corr_tmp_path), test_dir, tmp_prefix, corr_pat, dst_num);
  make_prefixed_path(info_src_path, sizeof(info_src_path), test_dir, src_prefix, info_pat, src_num);
  make_prefixed_path(info_dst_path, sizeof(info_dst_path), test_dir, dst_prefix, info_pat, dst_num);
  make_prefixed_path(info_tmp_path, sizeof(info_tmp_path), test_dir, tmp_prefix, info_pat, dst_num);

  if (logged_unlink(log_f, test_tmp_path) < 0) FAIL(S_ERR_FS_ERROR);
  if (logged_unlink(log_f, corr_tmp_path) < 0) FAIL(S_ERR_FS_ERROR);
  if (logged_unlink(log_f, info_tmp_path) < 0) FAIL(S_ERR_FS_ERROR);

  // DST->TMP
  if (logged_rename(log_f, test_dst_path, test_tmp_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, corr_dst_path, corr_tmp_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, info_dst_path, info_tmp_path) < 0) goto fs_error;
  ++stage;

  // SRC->DST
  if (logged_rename(log_f, test_src_path, test_dst_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, corr_src_path, corr_dst_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, info_src_path, info_dst_path) < 0) goto fs_error;
  ++stage;

  // TMP->SRC
  if (logged_rename(log_f, test_tmp_path, test_src_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, corr_tmp_path, corr_src_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, info_tmp_path, info_src_path) < 0) goto fs_error;
  ++stage;

cleanup:
  return retval;

fs_error:
  if (stage >= 9) logged_rename(log_f, info_src_path, info_tmp_path);
  if (stage >= 8) logged_rename(log_f, corr_src_path, corr_tmp_path);
  if (stage >= 7) logged_rename(log_f, test_src_path, test_tmp_path);
  if (stage >= 6) logged_rename(log_f, info_dst_path, info_src_path);
  if (stage >= 5) logged_rename(log_f, corr_dst_path, corr_src_path);
  if (stage >= 4) logged_rename(log_f, test_dst_path, test_src_path);
  if (stage >= 3) logged_rename(log_f, info_tmp_path, info_dst_path);
  if (stage >= 2) logged_rename(log_f, corr_tmp_path, corr_dst_path);
  if (stage >= 1) logged_rename(log_f, test_tmp_path, test_dst_path);
  FAIL(S_ERR_FS_ERROR);
}

static int
move_files(
        FILE *log_f,
        const unsigned char *test_dir,
        const unsigned char *test_pat,
        const unsigned char *corr_pat,
        const unsigned char *info_pat,
        const unsigned char *src_prefix,
        const unsigned char *dst_prefix,
        const unsigned char *tmp_prefix,
        int src_num,
        int dst_num)
{
  int retval = 0, stage = 0;
  unsigned char test_src_path[PATH_MAX];
  unsigned char test_dst_path[PATH_MAX];
  unsigned char test_tmp_path[PATH_MAX];
  unsigned char corr_src_path[PATH_MAX];
  unsigned char corr_dst_path[PATH_MAX];
  unsigned char corr_tmp_path[PATH_MAX];
  unsigned char info_src_path[PATH_MAX];
  unsigned char info_dst_path[PATH_MAX];
  unsigned char info_tmp_path[PATH_MAX];

  make_prefixed_path(test_src_path, sizeof(test_src_path), test_dir, src_prefix, test_pat, src_num);
  make_prefixed_path(test_dst_path, sizeof(test_dst_path), test_dir, dst_prefix, test_pat, dst_num);
  make_prefixed_path(test_tmp_path, sizeof(test_tmp_path), test_dir, tmp_prefix, test_pat, dst_num);
  make_prefixed_path(corr_src_path, sizeof(corr_src_path), test_dir, src_prefix, corr_pat, src_num);
  make_prefixed_path(corr_dst_path, sizeof(corr_dst_path), test_dir, dst_prefix, corr_pat, dst_num);
  make_prefixed_path(corr_tmp_path, sizeof(corr_tmp_path), test_dir, tmp_prefix, corr_pat, dst_num);
  make_prefixed_path(info_src_path, sizeof(info_src_path), test_dir, src_prefix, info_pat, src_num);
  make_prefixed_path(info_dst_path, sizeof(info_dst_path), test_dir, dst_prefix, info_pat, dst_num);
  make_prefixed_path(info_tmp_path, sizeof(info_tmp_path), test_dir, tmp_prefix, info_pat, dst_num);

  if (logged_unlink(log_f, test_tmp_path) < 0) FAIL(S_ERR_FS_ERROR);
  if (logged_unlink(log_f, corr_tmp_path) < 0) FAIL(S_ERR_FS_ERROR);
  if (logged_unlink(log_f, info_tmp_path) < 0) FAIL(S_ERR_FS_ERROR);

  // DST->TMP
  if (logged_rename(log_f, test_dst_path, test_tmp_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, corr_dst_path, corr_tmp_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, info_dst_path, info_tmp_path) < 0) goto fs_error;
  ++stage;

  // SRC->DST
  if (logged_rename(log_f, test_src_path, test_dst_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, corr_src_path, corr_dst_path) < 0) goto fs_error;
  ++stage;
  if (logged_rename(log_f, info_src_path, info_dst_path) < 0) goto fs_error;
  ++stage;

  // remove TMP
  logged_unlink(log_f, test_tmp_path);
  logged_unlink(log_f, corr_tmp_path);
  logged_unlink(log_f, info_tmp_path);

cleanup:
  return retval;

fs_error:
  if (stage >= 6) logged_rename(log_f, info_dst_path, info_src_path);
  if (stage >= 5) logged_rename(log_f, corr_dst_path, corr_src_path);
  if (stage >= 4) logged_rename(log_f, test_dst_path, test_src_path);
  if (stage >= 3) logged_rename(log_f, info_tmp_path, info_dst_path);
  if (stage >= 2) logged_rename(log_f, corr_tmp_path, corr_dst_path);
  if (stage >= 1) logged_rename(log_f, test_tmp_path, test_dst_path);
  FAIL(S_ERR_FS_ERROR);
}

static int
delete_test(
        FILE *log_f,
        const unsigned char *test_dir,
        const unsigned char *test_pat,
        const unsigned char *corr_pat,
        const unsigned char *info_pat,
        const unsigned char *prefix,
        int test_count, // 0-based
        int test_num) // 1-based
{
  int retval = 0;
  unsigned char test_src_path[PATH_MAX];
  unsigned char test_dst_path[PATH_MAX];
  unsigned char corr_src_path[PATH_MAX];
  unsigned char corr_dst_path[PATH_MAX];
  unsigned char info_src_path[PATH_MAX];
  unsigned char info_dst_path[PATH_MAX];

  if (test_num <= 0 || test_num >= test_count) return retval;

  make_prefixed_path(test_dst_path, sizeof(test_dst_path), test_dir, prefix, test_pat, test_num);
  make_prefixed_path(corr_dst_path, sizeof(corr_dst_path), test_dir, prefix, corr_pat, test_num);
  make_prefixed_path(info_dst_path, sizeof(info_dst_path), test_dir, prefix, info_pat, test_num);
  logged_unlink(log_f, test_dst_path);
  logged_unlink(log_f, corr_dst_path);
  logged_unlink(log_f, info_dst_path);

  for (++test_num; test_num <= test_count; ++test_num) {
    make_prefixed_path(test_dst_path, sizeof(test_dst_path), test_dir, prefix, test_pat, test_num - 1);
    make_prefixed_path(corr_dst_path, sizeof(corr_dst_path), test_dir, prefix, corr_pat, test_num - 1);
    make_prefixed_path(info_dst_path, sizeof(info_dst_path), test_dir, prefix, info_pat, test_num - 1);
    make_prefixed_path(test_src_path, sizeof(test_src_path), test_dir, prefix, test_pat, test_num);
    make_prefixed_path(corr_src_path, sizeof(corr_src_path), test_dir, prefix, corr_pat, test_num);
    make_prefixed_path(info_src_path, sizeof(info_src_path), test_dir, prefix, info_pat, test_num);
    // FIXME: check for errors
    logged_rename(log_f, test_src_path, test_dst_path);
    logged_rename(log_f, corr_src_path, corr_dst_path);
    logged_rename(log_f, info_src_path, info_dst_path);
  }

  return retval;
}

int
insert_test(
        FILE *log_f,
        const unsigned char *test_dir,
        const unsigned char *test_pat,
        const unsigned char *corr_pat,
        const unsigned char *info_pat,
        const unsigned char *prefix,
        int test_count, // 0-based
        int test_num) // 1-based
{
  int retval = 0, cur_test;
  unsigned char test_src_path[PATH_MAX];
  unsigned char test_dst_path[PATH_MAX];
  unsigned char corr_src_path[PATH_MAX];
  unsigned char corr_dst_path[PATH_MAX];
  unsigned char info_src_path[PATH_MAX];
  unsigned char info_dst_path[PATH_MAX];

  if (test_num <= 0 || test_num > test_count) return retval;

  for (cur_test = test_count; cur_test >= test_num; --cur_test) {
    make_prefixed_path(test_dst_path, sizeof(test_dst_path), test_dir, prefix, test_pat, cur_test + 1);
    make_prefixed_path(corr_dst_path, sizeof(corr_dst_path), test_dir, prefix, corr_pat, cur_test + 1);
    make_prefixed_path(info_dst_path, sizeof(info_dst_path), test_dir, prefix, info_pat, cur_test + 1);
    make_prefixed_path(test_src_path, sizeof(test_src_path), test_dir, prefix, test_pat, cur_test);
    make_prefixed_path(corr_src_path, sizeof(corr_src_path), test_dir, prefix, corr_pat, cur_test);
    make_prefixed_path(info_src_path, sizeof(info_src_path), test_dir, prefix, info_pat, cur_test);
    // FIXME: check for errors
    logged_rename(log_f, test_src_path, test_dst_path);
    logged_rename(log_f, corr_src_path, corr_dst_path);
    logged_rename(log_f, info_src_path, info_dst_path);
  }

  return retval;
}

static int
check_test_existance(
        FILE *log_f,
        const unsigned char *test_dir,
        const unsigned char *test_pat,
        const unsigned char *corr_pat,
        const unsigned char *info_pat,
        const unsigned char *prefix,
        int test_num) // 1-based
{
  unsigned char test_path[PATH_MAX];
  unsigned char corr_path[PATH_MAX];
  unsigned char info_path[PATH_MAX];
  int exists = 0;

  make_prefixed_path(test_path, sizeof(test_path), test_dir, prefix, test_pat, test_num);
  make_prefixed_path(corr_path, sizeof(corr_path), test_dir, prefix, corr_pat, test_num);
  make_prefixed_path(info_path, sizeof(info_path), test_dir, prefix, info_pat, test_num);

  if (test_path[0] && access(test_path, F_OK) >= 0) exists = 1;
  if (corr_path[0] && access(corr_path, F_OK) >= 0) exists = 1;
  if (info_path[0] && access(info_path, F_OK) >= 0) exists = 1;
  return exists;
}

int
super_serve_op_TESTS_TEST_MOVE_UP(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr)
{
  int retval = 0;
  int contest_id = 0;
  int prob_id = 0;
  int variant = 0;
  int test_num = 0;
  const struct contest_desc *cnts = NULL;
  opcap_t caps = 0LL;
  serve_state_t cs = NULL;
  const struct section_global_data *global = NULL;
  const struct section_problem_data *prob = NULL;
  unsigned char test_dir[PATH_MAX];
  unsigned char test_pat[PATH_MAX];
  unsigned char corr_pat[PATH_MAX];
  unsigned char info_pat[PATH_MAX];
  const unsigned char *pat_prefix = NULL;
  int from_test_num = 0;
  int to_test_num = 0;

  ss_cgi_param_int_opt(phr, "contest_id", &contest_id, 0);
  if (contest_id <= 0) FAIL(S_ERR_INV_CONTEST);
  if (contests_get(contest_id, &cnts) < 0 || !cnts) FAIL(S_ERR_INV_CONTEST);

  if (phr->priv_level < PRIV_LEVEL_JUDGE) FAIL(S_ERR_PERM_DENIED);
  get_full_caps(phr, cnts, &caps);
  if (opcaps_check(caps, OPCAP_CONTROL_CONTEST) < 0) FAIL(S_ERR_PERM_DENIED);

  retval = check_other_editors(log_f, out_f, phr, contest_id, cnts);
  if (retval <= 0) goto cleanup;
  retval = 0;
  cs = phr->ss->te_state;
  global = cs->global;

  ss_cgi_param_int_opt(phr, "prob_id", &prob_id, 0);
  if (prob_id <= 0 || prob_id > cs->max_prob) FAIL(S_ERR_INV_PROB_ID);
  if (!(prob = cs->probs[prob_id])) FAIL(S_ERR_INV_PROB_ID);

  variant = -1;
  if (prob->variant_num > 0) {
    ss_cgi_param_int_opt(phr, "variant", &variant, 0);
    if (variant <= 0 || variant > prob->variant_num) FAIL(S_ERR_INV_VARIANT);
  }

  ss_cgi_param_int_opt(phr, "test_num", &test_num, 0);
  if (test_num <= 0 || test_num >= 1000000) FAIL(S_ERR_INV_TEST_NUM);

  if (phr->opcode == SSERV_OP_TESTS_SAVED_MOVE_UP || phr->opcode == SSERV_OP_TESTS_SAVED_MOVE_DOWN) {
    pat_prefix = SAVED_TEST_PREFIX;
  }
  if (phr->opcode == SSERV_OP_TESTS_TEST_MOVE_UP || phr->opcode == SSERV_OP_TESTS_SAVED_MOVE_UP) {
    to_test_num = test_num - 1;
    from_test_num = test_num;
  } else if (phr->opcode == SSERV_OP_TESTS_TEST_MOVE_DOWN || phr->opcode == SSERV_OP_TESTS_SAVED_MOVE_DOWN) {
    to_test_num = test_num + 1;
    from_test_num = test_num;
  } else {
    FAIL(S_ERR_INV_OPER);
  }
  if (to_test_num <= 0 || from_test_num <= 0) goto done;

  retval = prepare_test_file_names(log_f, phr, cnts, global, prob, variant, pat_prefix,
                                   sizeof(test_dir), test_dir, test_pat, corr_pat, info_pat);
  if (retval < 0) goto cleanup;
  retval = 0;

  if (phr->opcode == SSERV_OP_TESTS_TEST_MOVE_DOWN || phr->opcode == SSERV_OP_TESTS_SAVED_MOVE_DOWN) {
    if (!check_test_existance(log_f, test_dir, test_pat, corr_pat, info_pat, pat_prefix, to_test_num))
      goto done;
  }

  retval = swap_files(log_f, test_dir, test_pat, corr_pat, info_pat, pat_prefix, pat_prefix, TEMP_TEST_PREFIX,
                      from_test_num, to_test_num);
  if (retval < 0) goto cleanup;
  retval = 0;

done:
  ss_redirect_2(out_f, phr, SSERV_OP_TESTS_TESTS_VIEW_PAGE, contest_id, prob_id, variant, 0);

cleanup:
  return retval;
}

int
super_serve_op_TESTS_TEST_MOVE_TO_SAVED(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr)
{
  int retval = 0;
  int contest_id = 0;
  int prob_id = 0;
  int variant = 0;
  int test_num = 0;
  const struct contest_desc *cnts = NULL;
  opcap_t caps = 0LL;
  serve_state_t cs = NULL;
  const struct section_global_data *global = NULL;
  const struct section_problem_data *prob = NULL;
  unsigned char test_dir[PATH_MAX];
  unsigned char test_pat[PATH_MAX];
  unsigned char corr_pat[PATH_MAX];
  unsigned char info_pat[PATH_MAX];
  struct test_dir_info td_info;

  memset(&td_info, 0, sizeof(td_info));
  ss_cgi_param_int_opt(phr, "contest_id", &contest_id, 0);
  if (contest_id <= 0) FAIL(S_ERR_INV_CONTEST);
  if (contests_get(contest_id, &cnts) < 0 || !cnts) FAIL(S_ERR_INV_CONTEST);

  if (phr->priv_level < PRIV_LEVEL_JUDGE) FAIL(S_ERR_PERM_DENIED);
  get_full_caps(phr, cnts, &caps);
  if (opcaps_check(caps, OPCAP_CONTROL_CONTEST) < 0) FAIL(S_ERR_PERM_DENIED);

  retval = check_other_editors(log_f, out_f, phr, contest_id, cnts);
  if (retval <= 0) goto cleanup;
  retval = 0;
  cs = phr->ss->te_state;
  global = cs->global;

  ss_cgi_param_int_opt(phr, "prob_id", &prob_id, 0);
  if (prob_id <= 0 || prob_id > cs->max_prob) FAIL(S_ERR_INV_PROB_ID);
  if (!(prob = cs->probs[prob_id])) FAIL(S_ERR_INV_PROB_ID);

  variant = -1;
  if (prob->variant_num > 0) {
    ss_cgi_param_int_opt(phr, "variant", &variant, 0);
    if (variant <= 0 || variant > prob->variant_num) FAIL(S_ERR_INV_VARIANT);
  }

  ss_cgi_param_int_opt(phr, "test_num", &test_num, 0);
  if (test_num <= 0 || test_num >= 1000000) FAIL(S_ERR_INV_TEST_NUM);

  retval = prepare_test_file_names(log_f, phr, cnts, global, prob, variant, NULL,
                                   sizeof(test_dir), test_dir, test_pat, corr_pat, info_pat);
  if (retval < 0) goto cleanup;
  retval = 0;

  retval = scan_test_directory(log_f, &td_info, cnts, test_dir, test_pat, corr_pat, info_pat);
  if (retval < 0) goto cleanup;

  if (phr->opcode == SSERV_OP_TESTS_TEST_MOVE_TO_SAVED) {
    if (test_num <= 0 || test_num > td_info.test_ref_count) goto done;
    if (move_files(log_f, test_dir, test_pat, corr_pat, info_pat, NULL, SAVED_TEST_PREFIX, TEMP_TEST_PREFIX,
                   test_num, td_info.saved_ref_count + 1) < 0)
      goto cleanup;
    if (delete_test(log_f, test_dir, test_pat, corr_pat, info_pat, NULL,
                    td_info.test_ref_count, test_num) < 0)
      goto cleanup;
  } else if (phr->opcode == SSERV_OP_TESTS_SAVED_MOVE_TO_TEST) {
    if (test_num <= 0 || test_num > td_info.saved_ref_count) goto done;
    if (move_files(log_f, test_dir, test_pat, corr_pat, info_pat, SAVED_TEST_PREFIX, NULL, TEMP_TEST_PREFIX,
                   test_num, td_info.test_ref_count + 1) < 0)
      goto cleanup;
    if (delete_test(log_f, test_dir, test_pat, corr_pat, info_pat, SAVED_TEST_PREFIX,
                    td_info.saved_ref_count, test_num) < 0)
      goto cleanup;
  } else {
    FAIL(S_ERR_INV_OPER);
  }

done:
  ss_redirect_2(out_f, phr, SSERV_OP_TESTS_TESTS_VIEW_PAGE, contest_id, prob_id, variant, 0);

cleanup:
  test_dir_info_free(&td_info);
  return retval;
}

int
super_serve_op_TESTS_TEST_EDIT_PAGE(
        FILE *log_f,
        FILE *out_f,
        struct super_http_request_info *phr)
{
  int retval = 0;
  int contest_id = 0;
  int prob_id = 0;
  int variant = 0;
  int test_num = 0;
  const struct contest_desc *cnts = NULL;
  opcap_t caps = 0LL;
  serve_state_t cs = NULL;
  const struct section_global_data *global = NULL;
  const struct section_problem_data *prob = NULL;
  unsigned char test_dir[PATH_MAX];
  unsigned char test_pat[PATH_MAX];
  unsigned char corr_pat[PATH_MAX];
  unsigned char info_pat[PATH_MAX];
  unsigned char buf[1024];
  unsigned char hbuf[1024];
  struct html_armor_buffer ab = HTML_ARMOR_INITIALIZER;

  ss_cgi_param_int_opt(phr, "contest_id", &contest_id, 0);
  if (contest_id <= 0) FAIL(S_ERR_INV_CONTEST);
  if (contests_get(contest_id, &cnts) < 0 || !cnts) FAIL(S_ERR_INV_CONTEST);

  if (phr->priv_level < PRIV_LEVEL_JUDGE) FAIL(S_ERR_PERM_DENIED);
  get_full_caps(phr, cnts, &caps);
  if (opcaps_check(caps, OPCAP_CONTROL_CONTEST) < 0) FAIL(S_ERR_PERM_DENIED);

  retval = check_other_editors(log_f, out_f, phr, contest_id, cnts);
  if (retval <= 0) goto cleanup;
  retval = 0;
  cs = phr->ss->te_state;
  global = cs->global;

  ss_cgi_param_int_opt(phr, "prob_id", &prob_id, 0);
  if (prob_id <= 0 || prob_id > cs->max_prob) FAIL(S_ERR_INV_PROB_ID);
  if (!(prob = cs->probs[prob_id])) FAIL(S_ERR_INV_PROB_ID);

  variant = -1;
  if (prob->variant_num > 0) {
    ss_cgi_param_int_opt(phr, "variant", &variant, 0);
    if (variant <= 0 || variant > prob->variant_num) FAIL(S_ERR_INV_VARIANT);
  }

  ss_cgi_param_int_opt(phr, "test_num", &test_num, 0);
  if (test_num <= 0 || test_num >= 1000000) FAIL(S_ERR_INV_TEST_NUM);

  retval = prepare_test_file_names(log_f, phr, cnts, global, prob, variant, NULL,
                                   sizeof(test_dir), test_dir, test_pat, corr_pat, info_pat);
  if (retval < 0) goto cleanup;
  retval = 0;

  snprintf(buf, sizeof(buf), "serve-control: %s, contest %d (%s), test %d for problem %s",
           phr->html_name, contest_id, ARMOR(cnts->name), test_num, prob->short_name);
  ss_write_html_header(out_f, phr, buf, 0, NULL);
  fprintf(out_f, "<h1>%s</h1>\n", buf);

  fprintf(out_f, "<ul>");
  fprintf(out_f, "<li>%s%s</a></li>",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL, NULL),
          "Main page");
  fprintf(out_f, "<li>%s%s</a></li>\n",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                        "action=%d&op=%d&contest_id=%d", SSERV_CMD_HTTP_REQUEST,
                        SSERV_OP_TESTS_MAIN_PAGE, contest_id),
          "Problems page");
  fprintf(out_f, "<li>%s%s</a></li>\n",
          html_hyperref(hbuf, sizeof(hbuf), phr->session_id, phr->self_url, NULL,
                        "action=%d&op=%d&contest_id=%d&prob_id=%d", SSERV_CMD_HTTP_REQUEST,
                        SSERV_OP_TESTS_TESTS_VIEW_PAGE, contest_id, prob_id),
          "Tests page");
  fprintf(out_f, "</ul>\n");

  html_start_form(out_f, 1, phr->self_url, "");
  html_hidden(out_f, "SID", "%016llx", phr->session_id);
  html_hidden(out_f, "action", "%d", SSERV_CMD_HTTP_REQUEST);
  html_hidden(out_f, "contest_id", "%d", contest_id);
  html_hidden(out_f, "prob_id", "%d", prob_id);
  html_hidden(out_f, "test_num", "%d", test_num);

  fprintf(out_f, "</form>\n");

  ss_write_html_footer(out_f);

cleanup:
  html_armor_free(&ab);
  return retval;
}
