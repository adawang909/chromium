// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_ping_manager.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

// Returns true if the |update_item| contains a valid differential update url.
bool HasDiffUpdate(const CrxUpdateItem* update_item) {
  return update_item->diff_crx_url.is_valid();
}

}  // namespace

namespace component_updater {

// Sends a fire and forget ping. The instances of this class have no
// ownership and they self-delete upon completion.
class PingSender : public net::URLFetcherDelegate {
 public:
  PingSender();

  void SendPing(const GURL& ping_url,
                net::URLRequestContextGetter* url_request_context_getter,
                const CrxUpdateItem* item);
 private:
  virtual ~PingSender();

  // Overrides for URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  static std::string BuildPing(const CrxUpdateItem* item);
  static std::string BuildPingEventElement(const CrxUpdateItem* item);

  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PingSender);
};

PingSender::PingSender() {}

PingSender::~PingSender() {}

void PingSender::OnURLFetchComplete(const net::URLFetcher* source) {
  delete this;
}

void PingSender::SendPing(
    const GURL& ping_url,
    net::URLRequestContextGetter* url_request_context_getter,
    const CrxUpdateItem* item) {
  DCHECK(item);

  if (!ping_url.is_valid())
    return;

  url_fetcher_.reset(SendProtocolRequest(ping_url,
                                         BuildPing(item),
                                         this,
                                         url_request_context_getter));
}

// Builds a ping message for the specified update item.
std::string PingSender::BuildPing(const CrxUpdateItem* item) {
  const char app_element_format[] =
      "<app appid=\"%s\" version=\"%s\" nextversion=\"%s\">"
      "%s"
      "</app>";
  const std::string app_element(base::StringPrintf(
      app_element_format,
      item->id.c_str(),                               // "appid"
      item->previous_version.GetString().c_str(),     // "version"
      item->next_version.GetString().c_str(),         // "nextversion"
      BuildPingEventElement(item).c_str()));

  return BuildProtocolRequest(app_element);
}

// Returns a string representing one ping event xml element for an update item.
std::string PingSender::BuildPingEventElement(const CrxUpdateItem* item) {
  DCHECK(item->status == CrxUpdateItem::kNoUpdate ||
         item->status == CrxUpdateItem::kUpdated);

  using base::StringAppendF;

  std::string ping_event("<event eventtype=\"3\"");
  const int event_result = item->status == CrxUpdateItem::kUpdated;
  StringAppendF(&ping_event, " eventresult=\"%d\"", event_result);
  if (item->error_category)
    StringAppendF(&ping_event, " errorcat=\"%d\"", item->error_category);
  if (item->error_code)
    StringAppendF(&ping_event, " errorcode=\"%d\"", item->error_code);
  if (item->extra_code1)
    StringAppendF(&ping_event, " extracode1=\"%d\"", item->extra_code1);
  if (HasDiffUpdate(item))
    StringAppendF(&ping_event, " diffresult=\"%d\"", !item->diff_update_failed);
  if (item->diff_error_category)
    StringAppendF(&ping_event,
                  " differrorcat=\"%d\"",
                  item->diff_error_category);
  if (item->diff_error_code)
    StringAppendF(&ping_event, " differrorcode=\"%d\"", item->diff_error_code);
  if (item->diff_extra_code1) {
    StringAppendF(&ping_event,
                  " diffextracode1=\"%d\"",
                  item->diff_extra_code1);
  }
  if (!item->previous_fp.empty())
    StringAppendF(&ping_event, " previousfp=\"%s\"", item->previous_fp.c_str());
  if (!item->next_fp.empty())
    StringAppendF(&ping_event, " nextfp=\"%s\"", item->next_fp.c_str());
  StringAppendF(&ping_event, "/>");
  return ping_event;
}

PingManager::PingManager(
    const GURL& ping_url,
    net::URLRequestContextGetter* url_request_context_getter)
    : ping_url_(ping_url),
      url_request_context_getter_(url_request_context_getter) {
}

PingManager::~PingManager() {
}

// Sends a fire and forget ping when the updates are complete. The ping
// sender object self-deletes after sending the ping.
void PingManager::OnUpdateComplete(const CrxUpdateItem* item) {
  component_updater::PingSender* ping_sender(new PingSender);
  ping_sender->SendPing(ping_url_, url_request_context_getter_, item);
}

}  // namespace component_updater

