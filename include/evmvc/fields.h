/*
MIT License

Copyright (c) 2019 Michel Dénommée

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// copy and modified from: https://github.com/boostorg/beast

#ifndef _libevmvc_fields_h
#define _libevmvc_fields_h

#include "stable_headers.h"
#include <boost/assert.hpp>

namespace evmvc {

enum class field : unsigned short
{
    unknown = 0,

    a_im,
    accept,
    accept_additions,
    accept_charset,
    accept_datetime,
    accept_encoding,
    accept_features,
    accept_language,
    accept_patch,
    accept_post,
    accept_ranges,
    access_control,
    access_control_allow_credentials,
    access_control_allow_headers,
    access_control_allow_methods,
    access_control_allow_origin,
    access_control_expose_headers,
    access_control_max_age,
    access_control_request_headers,
    access_control_request_method,
    age,
    allow,
    alpn,
    also_control,
    alt_svc,
    alt_used,
    alternate_recipient,
    alternates,
    apparently_to,
    apply_to_redirect_ref,
    approved,
    archive,
    archived_at,
    article_names,
    article_updates,
    authentication_control,
    authentication_info,
    authentication_results,
    authorization,
    auto_submitted,
    autoforwarded,
    autosubmitted,
    base,
    bcc,
    body,
    c_ext,
    c_man,
    c_opt,
    c_pep,
    c_pep_info,
    cache_control,
    caldav_timezones,
    cancel_key,
    cancel_lock,
    cc,
    close,
    comments,
    compliance,
    connection,
    content_alternative,
    content_base,
    content_description,
    content_disposition,
    content_duration,
    content_encoding,
    content_features,
    content_id,
    content_identifier,
    content_language,
    content_length,
    content_location,
    content_md5,
    content_range,
    content_return,
    content_script_type,
    content_style_type,
    content_transfer_encoding,
    content_type,
    content_version,
    control,
    conversion,
    conversion_with_loss,
    cookie,
    cookie2,
    cost,
    dasl,
    date,
    date_received,
    dav,
    default_style,
    deferred_delivery,
    delivery_date,
    delta_base,
    depth,
    derived_from,
    destination,
    differential_id,
    digest,
    discarded_x400_ipms_extensions,
    discarded_x400_mts_extensions,
    disclose_recipients,
    disposition_notification_options,
    disposition_notification_to,
    distribution,
    dkim_signature,
    dl_expansion_history,
    downgraded_bcc,
    downgraded_cc,
    downgraded_disposition_notification_to,
    downgraded_final_recipient,
    downgraded_from,
    downgraded_in_reply_to,
    downgraded_mail_from,
    downgraded_message_id,
    downgraded_original_recipient,
    downgraded_rcpt_to,
    downgraded_references,
    downgraded_reply_to,
    downgraded_resent_bcc,
    downgraded_resent_cc,
    downgraded_resent_from,
    downgraded_resent_reply_to,
    downgraded_resent_sender,
    downgraded_resent_to,
    downgraded_return_path,
    downgraded_sender,
    downgraded_to,
    ediint_features,
    eesst_version,
    encoding,
    encrypted,
    errors_to,
    etag,
    expect,
    expires,
    expiry_date,
    ext,
    followup_to,
    forwarded,
    from,
    generate_delivery_report,
    getprofile,
    hobareg,
    host,
    http2_settings,
    if_,
    if_match,
    if_modified_since,
    if_none_match,
    if_range,
    if_schedule_tag_match,
    if_unmodified_since,
    im,
    importance,
    in_reply_to,
    incomplete_copy,
    injection_date,
    injection_info,
    jabber_id,
    keep_alive,
    keywords,
    label,
    language,
    last_modified,
    latest_delivery_time,
    lines,
    link,
    list_archive,
    list_help,
    list_id,
    list_owner,
    list_post,
    list_subscribe,
    list_unsubscribe,
    list_unsubscribe_post,
    location,
    lock_token,
    man,
    max_forwards,
    memento_datetime,
    message_context,
    message_id,
    message_type,
    meter,
    method_check,
    method_check_expires,
    mime_version,
    mmhs_acp127_message_identifier,
    mmhs_authorizing_users,
    mmhs_codress_message_indicator,
    mmhs_copy_precedence,
    mmhs_exempted_address,
    mmhs_extended_authorisation_info,
    mmhs_handling_instructions,
    mmhs_message_instructions,
    mmhs_message_type,
    mmhs_originator_plad,
    mmhs_originator_reference,
    mmhs_other_recipients_indicator_cc,
    mmhs_other_recipients_indicator_to,
    mmhs_primary_precedence,
    mmhs_subject_indicator_codes,
    mt_priority,
    negotiate,
    newsgroups,
    nntp_posting_date,
    nntp_posting_host,
    non_compliance,
    obsoletes,
    opt,
    optional,
    optional_www_authenticate,
    ordering_type,
    organization,
    origin,
    original_encoded_information_types,
    original_from,
    original_message_id,
    original_recipient,
    original_sender,
    original_subject,
    originator_return_address,
    overwrite,
    p3p,
    path,
    pep,
    pep_info,
    pics_label,
    position,
    posting_version,
    pragma,
    prefer,
    preference_applied,
    prevent_nondelivery_report,
    priority,
    privicon,
    profileobject,
    protocol,
    protocol_info,
    protocol_query,
    protocol_request,
    proxy_authenticate,
    proxy_authentication_info,
    proxy_authorization,
    proxy_connection,
    proxy_features,
    proxy_instruction,
    public_,
    public_key_pins,
    public_key_pins_report_only,
    range,
    received,
    received_spf,
    redirect_ref,
    references,
    referer,
    referer_root,
    relay_version,
    reply_by,
    reply_to,
    require_recipient_valid_since,
    resent_bcc,
    resent_cc,
    resent_date,
    resent_from,
    resent_message_id,
    resent_reply_to,
    resent_sender,
    resent_to,
    resolution_hint,
    resolver_location,
    retry_after,
    return_path,
    safe,
    schedule_reply,
    schedule_tag,
    sec_websocket_accept,
    sec_websocket_extensions,
    sec_websocket_key,
    sec_websocket_protocol,
    sec_websocket_version,
    security_scheme,
    see_also,
    sender,
    sensitivity,
    server,
    set_cookie,
    set_cookie2,
    setprofile,
    sio_label,
    sio_label_history,
    slug,
    soapaction,
    solicitation,
    status_uri,
    strict_transport_security,
    subject,
    subok,
    subst,
    summary,
    supersedes,
    surrogate_capability,
    surrogate_control,
    tcn,
    te,
    timeout,
    title,
    to,
    topic,
    trailer,
    transfer_encoding,
    ttl,
    ua_color,
    ua_media,
    ua_pixels,
    ua_resolution,
    ua_windowpixels,
    upgrade,
    urgency,
    uri,
    user_agent,
    variant_vary,
    vary,
    vbr_info,
    version,
    via,
    want_digest,
    warning,
    www_authenticate,
    x_archived_at,
    x_device_accept,
    x_device_accept_charset,
    x_device_accept_encoding,
    x_device_accept_language,
    x_device_user_agent,
    x_frame_options,
    x_mittente,
    x_pgp_sig,
    x_ricevuta,
    x_riferimento_message_id,
    x_tiporicevuta,
    x_trasporto,
    x_verificasicurezza,
    x400_content_identifier,
    x400_content_return,
    x400_content_type,
    x400_mts_identifier,
    x400_originator,
    x400_received,
    x400_recipients,
    x400_trace,
    xref,
};

namespace detail {

inline char ascii_tolower(char c)
{
    return ((static_cast<unsigned>(c) - 65U) < 26) ?
        c + 'a' - 'A' : c;
}

struct field_table
{
    using array_type =
        std::array<md::string_view, 353>;

    struct hash
    {
        std::size_t
        operator()(md::string_view s) const
        {
            auto const n = s.size();
            return
                ascii_tolower(s[0]) *
                ascii_tolower(s[n/2]) ^
                ascii_tolower(s[n-1]);   // hist[] = 331, 10, max_load_factor = 0.15f
        }
    };

    struct iequal
    {
        // assumes inputs have equal length
        bool
        operator()(
            md::string_view lhs,
            md::string_view rhs) const
        {
            auto p1 = lhs.data();
            auto p2 = rhs.data();
            auto pend = p1 + lhs.size();
            char a, b;
            while(p1 < pend)
            {
                a = *p1++;
                b = *p2++;
                if(a != b)
                    goto slow;
            }
            return true;

            while(p1 < pend)
            {
            slow:
                if(ascii_tolower(a) != ascii_tolower(b))
                    return false;
                a = *p1++;
                b = *p2++;
            }
            return true;
        }
    };

    using map_type = std::unordered_map<
        md::string_view, field, hash, iequal>;

    array_type by_name_;
    std::vector<map_type> by_size_;
/*
    From:
    
    https://www.iana.org/assignments/message-headers/message-headers.xhtml
*/
    field_table()
        : by_name_({{
            "<unknown-field>",
            "A-IM",
            "Accept",
            "Accept-Additions",
            "Accept-Charset",
            "Accept-Datetime",
            "Accept-Encoding",
            "Accept-Features",
            "Accept-Language",
            "Accept-Patch",
            "Accept-Post",
            "Accept-Ranges",
            "Access-Control",
            "Access-Control-Allow-Credentials",
            "Access-Control-Allow-Headers",
            "Access-Control-Allow-Methods",
            "Access-Control-Allow-Origin",
            "Access-Control-Expose-Headers",
            "Access-Control-Max-Age",
            "Access-Control-Request-Headers",
            "Access-Control-Request-Method",
            "Age",
            "Allow",
            "ALPN",
            "Also-Control",
            "Alt-Svc",
            "Alt-Used",
            "Alternate-Recipient",
            "Alternates",
            "Apparently-To",
            "Apply-To-Redirect-Ref",
            "Approved",
            "Archive",
            "Archived-At",
            "Article-Names",
            "Article-Updates",
            "Authentication-Control",
            "Authentication-Info",
            "Authentication-Results",
            "Authorization",
            "Auto-Submitted",
            "Autoforwarded",
            "Autosubmitted",
            "Base",
            "Bcc",
            "Body",
            "C-Ext",
            "C-Man",
            "C-Opt",
            "C-PEP",
            "C-PEP-Info",
            "Cache-Control",
            "CalDAV-Timezones",
            "Cancel-Key",
            "Cancel-Lock",
            "Cc",
            "Close",
            "Comments",
            "Compliance",
            "Connection",
            "Content-Alternative",
            "Content-Base",
            "Content-Description",
            "Content-Disposition",
            "Content-Duration",
            "Content-Encoding",
            "Content-features",
            "Content-ID",
            "Content-Identifier",
            "Content-Language",
            "Content-Length",
            "Content-Location",
            "Content-MD5",
            "Content-Range",
            "Content-Return",
            "Content-Script-Type",
            "Content-Style-Type",
            "Content-Transfer-Encoding",
            "Content-Type",
            "Content-Version",
            "Control",
            "Conversion",
            "Conversion-With-Loss",
            "Cookie",
            "Cookie2",
            "Cost",
            "DASL",
            "Date",
            "Date-Received",
            "DAV",
            "Default-Style",
            "Deferred-Delivery",
            "Delivery-Date",
            "Delta-Base",
            "Depth",
            "Derived-From",
            "Destination",
            "Differential-ID",
            "Digest",
            "Discarded-X400-IPMS-Extensions",
            "Discarded-X400-MTS-Extensions",
            "Disclose-Recipients",
            "Disposition-Notification-Options",
            "Disposition-Notification-To",
            "Distribution",
            "DKIM-Signature",
            "DL-Expansion-History",
            "Downgraded-Bcc",
            "Downgraded-Cc",
            "Downgraded-Disposition-Notification-To",
            "Downgraded-Final-Recipient",
            "Downgraded-From",
            "Downgraded-In-Reply-To",
            "Downgraded-Mail-From",
            "Downgraded-Message-Id",
            "Downgraded-Original-Recipient",
            "Downgraded-Rcpt-To",
            "Downgraded-References",
            "Downgraded-Reply-To",
            "Downgraded-Resent-Bcc",
            "Downgraded-Resent-Cc",
            "Downgraded-Resent-From",
            "Downgraded-Resent-Reply-To",
            "Downgraded-Resent-Sender",
            "Downgraded-Resent-To",
            "Downgraded-Return-Path",
            "Downgraded-Sender",
            "Downgraded-To",
            "EDIINT-Features",
            "Eesst-Version",
            "Encoding",
            "Encrypted",
            "Errors-To",
            "ETag",
            "Expect",
            "Expires",
            "Expiry-Date",
            "Ext",
            "Followup-To",
            "Forwarded",
            "From",
            "Generate-Delivery-Report",
            "GetProfile",
            "Hobareg",
            "Host",
            "HTTP2-Settings",
            "If",
            "If-Match",
            "If-Modified-Since",
            "If-None-Match",
            "If-Range",
            "If-Schedule-Tag-Match",
            "If-Unmodified-Since",
            "IM",
            "Importance",
            "In-Reply-To",
            "Incomplete-Copy",
            "Injection-Date",
            "Injection-Info",
            "Jabber-ID",
            "Keep-Alive",
            "Keywords",
            "Label",
            "Language",
            "Last-Modified",
            "Latest-Delivery-Time",
            "Lines",
            "Link",
            "List-Archive",
            "List-Help",
            "List-ID",
            "List-Owner",
            "List-Post",
            "List-Subscribe",
            "List-Unsubscribe",
            "List-Unsubscribe-Post",
            "Location",
            "Lock-Token",
            "Man",
            "Max-Forwards",
            "Memento-Datetime",
            "Message-Context",
            "Message-ID",
            "Message-Type",
            "Meter",
            "Method-Check",
            "Method-Check-Expires",
            "MIME-Version",
            "MMHS-Acp127-Message-Identifier",
            "MMHS-Authorizing-Users",
            "MMHS-Codress-Message-Indicator",
            "MMHS-Copy-Precedence",
            "MMHS-Exempted-Address",
            "MMHS-Extended-Authorisation-Info",
            "MMHS-Handling-Instructions",
            "MMHS-Message-Instructions",
            "MMHS-Message-Type",
            "MMHS-Originator-PLAD",
            "MMHS-Originator-Reference",
            "MMHS-Other-Recipients-Indicator-CC",
            "MMHS-Other-Recipients-Indicator-To",
            "MMHS-Primary-Precedence",
            "MMHS-Subject-Indicator-Codes",
            "MT-Priority",
            "Negotiate",
            "Newsgroups",
            "NNTP-Posting-Date",
            "NNTP-Posting-Host",
            "Non-Compliance",
            "Obsoletes",
            "Opt",
            "Optional",
            "Optional-WWW-Authenticate",
            "Ordering-Type",
            "Organization",
            "Origin",
            "Original-Encoded-Information-Types",
            "Original-From",
            "Original-Message-ID",
            "Original-Recipient",
            "Original-Sender",
            "Original-Subject",
            "Originator-Return-Address",
            "Overwrite",
            "P3P",
            "Path",
            "PEP",
            "Pep-Info",
            "PICS-Label",
            "Position",
            "Posting-Version",
            "Pragma",
            "Prefer",
            "Preference-Applied",
            "Prevent-NonDelivery-Report",
            "Priority",
            "Privicon",
            "ProfileObject",
            "Protocol",
            "Protocol-Info",
            "Protocol-Query",
            "Protocol-Request",
            "Proxy-Authenticate",
            "Proxy-Authentication-Info",
            "Proxy-Authorization",
            "Proxy-Connection",
            "Proxy-Features",
            "Proxy-Instruction",
            "Public",
            "Public-Key-Pins",
            "Public-Key-Pins-Report-Only",
            "Range",
            "Received",
            "Received-SPF",
            "Redirect-Ref",
            "References",
            "Referer",
            "Referer-Root",
            "Relay-Version",
            "Reply-By",
            "Reply-To",
            "Require-Recipient-Valid-Since",
            "Resent-Bcc",
            "Resent-Cc",
            "Resent-Date",
            "Resent-From",
            "Resent-Message-ID",
            "Resent-Reply-To",
            "Resent-Sender",
            "Resent-To",
            "Resolution-Hint",
            "Resolver-Location",
            "Retry-After",
            "Return-Path",
            "Safe",
            "Schedule-Reply",
            "Schedule-Tag",
            "Sec-WebSocket-Accept",
            "Sec-WebSocket-Extensions",
            "Sec-WebSocket-Key",
            "Sec-WebSocket-Protocol",
            "Sec-WebSocket-Version",
            "Security-Scheme",
            "See-Also",
            "Sender",
            "Sensitivity",
            "Server",
            "Set-Cookie",
            "Set-Cookie2",
            "SetProfile",
            "SIO-Label",
            "SIO-Label-History",
            "SLUG",
            "SoapAction",
            "Solicitation",
            "Status-URI",
            "Strict-Transport-Security",
            "Subject",
            "SubOK",
            "Subst",
            "Summary",
            "Supersedes",
            "Surrogate-Capability",
            "Surrogate-Control",
            "TCN",
            "TE",
            "Timeout",
            "Title",
            "To",
            "Topic",
            "Trailer",
            "Transfer-Encoding",
            "TTL",
            "UA-Color",
            "UA-Media",
            "UA-Pixels",
            "UA-Resolution",
            "UA-Windowpixels",
            "Upgrade",
            "Urgency",
            "URI",
            "User-Agent",
            "Variant-Vary",
            "Vary",
            "VBR-Info",
            "Version",
            "Via",
            "Want-Digest",
            "Warning",
            "WWW-Authenticate",
            "X-Archived-At",
            "X-Device-Accept",
            "X-Device-Accept-Charset",
            "X-Device-Accept-Encoding",
            "X-Device-Accept-Language",
            "X-Device-User-Agent",
            "X-Frame-Options",
            "X-Mittente",
            "X-PGP-Sig",
            "X-Ricevuta",
            "X-Riferimento-Message-ID",
            "X-TipoRicevuta",
            "X-Trasporto",
            "X-VerificaSicurezza",
            "X400-Content-Identifier",
            "X400-Content-Return",
            "X400-Content-Type",
            "X400-MTS-Identifier",
            "X400-Originator",
            "X400-Received",
            "X400-Recipients",
            "X400-Trace",
            "Xref"
        }})
    {
        // find the longest field length
        std::size_t high = 0;
        for(auto const& s : by_name_)
            if(high < s.size())
                high = s.size();
        // build by_size map
        // skip field::unknown
        by_size_.resize(high + 1);
        for(auto& map : by_size_)
            map.max_load_factor(.15f);
        for(std::size_t i = 1;
            i < by_name_.size(); ++i)
        {
            auto const& s = by_name_[i];
            by_size_[s.size()].emplace(
                s, static_cast<field>(i));
        }

#if 0
        // This snippet calculates the performance
        // of the hash function and map settings
        {
            std::vector<std::size_t> hist;
            for(auto const& map : by_size_)
            {
                for(std::size_t i = 0; i < map.bucket_count(); ++i)
                {
                    auto const n = map.bucket_size(i);
                    if(n > 0)
                    {
                        if(hist.size() < n)
                            hist.resize(n);
                        ++hist[n-1];
                    }
                }
            }
        }
#endif
    }

    field
    string_to_field(md::string_view s) const
    {
        if(s.size() >= by_size_.size())
            return field::unknown;
        auto const& map = by_size_[s.size()];
        if(map.empty())
            return field::unknown;
        auto it = map.find(s);
        if(it == map.end())
            return field::unknown;
        return it->second;
    }

    //
    // Deprecated
    //

    using const_iterator =
    array_type::const_iterator; 

    std::size_t
    size() const
    {
        return by_name_.size();
    }

    const_iterator
    begin() const
    {
        return by_name_.begin();
    }

    const_iterator
    end() const
    {
        return by_name_.end();
    }
};

inline
field_table const&
get_field_table()
{
    static field_table const tab;
    return tab;
}

template<class = void>
md::string_view
to_string(field f)
{
    auto const& v = get_field_table();
    BOOST_ASSERT(static_cast<unsigned>(f) < v.size());
    return v.begin()[static_cast<unsigned>(f)];
}

} //ns evmvc::detail


/** Convert a field enum to a string.
    @param f The field to convert
*/
inline md::string_view to_string(field f)
{
    return detail::to_string(f);
}

/** Attempt to convert a string to a field enum.
    The string comparison is case-insensitive.
    @return The corresponding field, or @ref field::unknown
    if no known field matches.
*/
inline field string_to_field(md::string_view s)
{
    return detail::get_field_table().string_to_field(s);
}    

/// Write the text for a field name to an output stream.
inline std::ostream& operator<<(std::ostream& os, field f)
{
    return os << to_string(f);
}


} //ns evmvc
#endif //_libevmvc_fields_h
