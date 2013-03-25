/*
 * Copyright (c) 2012, Nasel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
 
#include <cassert>
#include <cstring>
#include "icmpv6.h"
#include "ipv6.h"
#include "rawpdu.h"
#include "utils.h"
#include "constants.h"

namespace Tins {

ICMPv6::ICMPv6(Types tp)
: _options_size(), reach_time(0), retrans_timer(0)
{
    std::memset(&_header, 0, sizeof(_header));
    type(tp);
}

ICMPv6::ICMPv6(const uint8_t *buffer, uint32_t total_sz) 
: _options_size(), reach_time(0), retrans_timer(0)
{
    if(total_sz < sizeof(_header))
        throw std::runtime_error("Not enough size for an ICMPv6 header");
    std::memcpy(&_header, buffer, sizeof(_header));
    buffer += sizeof(_header);
    total_sz -= sizeof(_header);
    if(has_target_addr()) {
        if(total_sz < ipaddress_type::address_size)
            throw std::runtime_error("Not enough size for the target address");
        target_addr(buffer);
        buffer += ipaddress_type::address_size;
        total_sz -= ipaddress_type::address_size;
    }
    if(has_dest_addr()) {
        if(total_sz < ipaddress_type::address_size)
            throw std::runtime_error("Not enough size for the destination address");
        dest_addr(buffer);
        buffer += ipaddress_type::address_size;
        total_sz -= ipaddress_type::address_size;
    }
    if(type() == ROUTER_ADVERT) {
        if(total_sz < sizeof(uint32_t) * 2)
            throw std::runtime_error("Not enough size for router advert fields");
        const uint32_t *ptr_32 = (const uint32_t*)buffer;
        reach_time = *ptr_32++;
        retrans_timer = *ptr_32++;
        
        buffer += sizeof(uint32_t) * 2;
        total_sz -= sizeof(uint32_t) * 2;
    }
    if(has_options())
        parse_options(buffer, total_sz);
    if(total_sz > 0)
        inner_pdu(new RawPDU(buffer, total_sz));
}

void ICMPv6::parse_options(const uint8_t *&buffer, uint32_t &total_sz) {
    while(total_sz > 0) {
        if(total_sz < 8 || (static_cast<uint32_t>(buffer[1]) * 8) > total_sz || buffer[1] < 1) 
            throw std::runtime_error("Not enough size for options");
        // size(option) = option_size - identifier_size - length_identifier_size
        add_option(icmpv6_option(buffer[0], static_cast<uint32_t>(buffer[1]) * 8 - sizeof(uint8_t) * 2, buffer + 2));
        total_sz -= buffer[1] * 8;
        buffer += buffer[1] * 8;
    }
}

void ICMPv6::type(Types new_type) {
    _header.type = new_type;
}

void ICMPv6::code(uint8_t new_code) {
    _header.code = new_code;
}

void ICMPv6::checksum(uint16_t new_cksum) {
    _header.cksum = Endian::host_to_be(new_cksum);
}

void ICMPv6::identifier(uint16_t new_identifier) {
    _header.u_echo.identifier = Endian::host_to_be(new_identifier);
}

void ICMPv6::sequence(uint16_t new_sequence) {
    _header.u_echo.sequence = Endian::host_to_be(new_sequence);
}

void ICMPv6::override(small_uint<1> new_override) {
    _header.u_nd_advt.override = new_override;
}

void ICMPv6::solicited(small_uint<1> new_solicited) {
    _header.u_nd_advt.solicited = new_solicited;
}

void ICMPv6::router(small_uint<1> new_router) {
    _header.u_nd_advt.router = new_router;
}

void ICMPv6::hop_limit(uint8_t new_hop_limit) {
    _header.u_nd_ra.hop_limit = new_hop_limit;
}

void ICMPv6::router_pref(small_uint<2> new_router_pref) {
    _header.u_nd_ra.router_pref = new_router_pref;
}

void ICMPv6::home_agent(small_uint<1> new_home_agent) {
    _header.u_nd_ra.home_agent = new_home_agent;
}

void ICMPv6::other(small_uint<1> new_other) {
    _header.u_nd_ra.other = new_other;
}

void ICMPv6::managed(small_uint<1> new_managed) {
    _header.u_nd_ra.managed = new_managed;
}

void ICMPv6::router_lifetime(uint16_t new_router_lifetime) {
    _header.u_nd_ra.router_lifetime = Endian::host_to_be(new_router_lifetime);
}

void ICMPv6::reachable_time(uint32_t new_reachable_time) {
    reach_time = Endian::host_to_be(new_reachable_time);
}

void ICMPv6::retransmit_timer(uint32_t new_retrans_timer) {
    retrans_timer = Endian::host_to_be(new_retrans_timer);
}

void ICMPv6::target_addr(const ipaddress_type &new_target_addr) {
    _target_address = new_target_addr;
}

void ICMPv6::dest_addr(const ipaddress_type &new_dest_addr) {
    _dest_address = new_dest_addr;
}

uint32_t ICMPv6::header_size() const {
    uint32_t extra = 0;
    if(type() == ROUTER_ADVERT)
        extra = sizeof(uint32_t) * 2;
    return sizeof(_header) + _options_size + extra + 
        (has_target_addr() ? ipaddress_type::address_size : 0) +
        (has_dest_addr() ? ipaddress_type::address_size : 0);
}

void ICMPv6::write_serialization(uint8_t *buffer, uint32_t total_sz, const PDU *parent) {
    #ifdef TINS_DEBUG
    assert(total_sz >= header_size());
    #endif
    icmp6hdr* ptr_header = (icmp6hdr*)buffer;
    std::memcpy(buffer, &_header, sizeof(_header));
    buffer += sizeof(_header);
    total_sz -= sizeof(_header);
    if(has_target_addr()) {
        buffer = _target_address.copy(buffer);
        total_sz -= sizeof(ipaddress_type::address_size);
    }
    if(has_dest_addr()) {
        buffer = _dest_address.copy(buffer);
        total_sz -= sizeof(ipaddress_type::address_size);
    }
    if(type() == ROUTER_ADVERT) {
        *(uint32_t*)buffer = reach_time;
        buffer += sizeof(uint32_t);
        *(uint32_t*)buffer = retrans_timer;
        buffer += sizeof(uint32_t);
        total_sz -= sizeof(uint32_t) * 2;
    }
    for(options_type::const_iterator it = _options.begin(); it != _options.end(); ++it) {
        #ifdef TINS_DEBUG
        assert(total_sz >= it->data_size() + sizeof(uint8_t) * 2);
        total_sz -= it->data_size() + sizeof(uint8_t) * 2;
        #endif
        buffer = write_option(*it, buffer);
    }
    if(!_header.cksum) {
        const Tins::IPv6 *ipv6 = dynamic_cast<const Tins::IPv6*>(parent);
        if(ipv6) {
            uint32_t checksum = Utils::pseudoheader_checksum(
                                    ipv6->src_addr(),  
                                    ipv6->dst_addr(), 
                                    size(), 
                                    Constants::IP::PROTO_ICMPV6
                                ) + Utils::do_checksum((uint8_t*)ptr_header, buffer);
            while (checksum >> 16) 
                checksum = (checksum & 0xffff) + (checksum >> 16);
            ptr_header->cksum = Endian::host_to_be<uint16_t>(~checksum);
        }
    }
}

// can i haz more?
bool ICMPv6::has_options() const {
    return type() == NEIGHBOUR_SOLICIT ||
            type() == ROUTER_ADVERT;
}

void ICMPv6::add_option(const icmpv6_option &option) {
    internal_add_option(option);
    _options.push_back(option);
}

#if TINS_IS_CXX11
void ICMPv6::add_option(icmpv6_option &&option) {
    internal_add_option(option);
    _options.push_back(std::move(option));
}
#endif

void ICMPv6::internal_add_option(const icmpv6_option &option) {
    _options_size += option.data_size() + sizeof(uint8_t) * 2;
}

uint8_t *ICMPv6::write_option(const icmpv6_option &opt, uint8_t *buffer) {
    *buffer++ = opt.option();
    *buffer++ = (opt.data_size() + sizeof(uint8_t) * 2) / 8;
    return std::copy(opt.data_ptr(), opt.data_ptr() + opt.data_size(), buffer);
}

const ICMPv6::icmpv6_option *ICMPv6::search_option(Options id) const {
    for(options_type::const_iterator it = _options.begin(); it != _options.end(); ++it) {
        if(it->option() == id)
            return &*it;
    }
    return 0;
}

// ********************************************************************
//                          Option setters
// ********************************************************************

void ICMPv6::source_link_layer_addr(const hwaddress_type &addr) {
    add_option(icmpv6_option(SOURCE_ADDRESS, addr.begin(), addr.end()));
}

void ICMPv6::target_link_layer_addr(const hwaddress_type &addr) {
    add_option(icmpv6_option(TARGET_ADDRESS, addr.begin(), addr.end()));
}

void ICMPv6::prefix_info(prefix_info_type info) {
    uint8_t buffer[2 + sizeof(uint32_t) * 3 + ipaddress_type::address_size];
    buffer[0] = info.prefix_len;
    buffer[1] = (info.L << 7) | (info.A << 6);
    *(uint32_t*)(buffer + 2) = Endian::host_to_be(info.valid_lifetime);
    *(uint32_t*)(buffer + 2 + sizeof(uint32_t)) = Endian::host_to_be(info.preferred_lifetime);
    *(uint32_t*)(buffer + 2 + sizeof(uint32_t) * 2) = 0;
    info.prefix.copy(buffer + 2 + sizeof(uint32_t) * 3);
    add_option(
        icmpv6_option(PREFIX_INFO, buffer, buffer + sizeof(buffer))
    );
}

void ICMPv6::redirect_header(PDU::serialization_type data) {
    // Reserved fields
    data.insert(data.begin(), 6, 0);
    // Padding(if necessary)
    uint8_t padding = 8 - (data.size() + sizeof(uint8_t) * 2) % 8;
    if(padding == 8)
        padding = 0;
    data.insert(data.end(), padding, 0);
    add_option(icmpv6_option(REDIRECT_HEADER, data.begin(), data.end()));
}

void ICMPv6::mtu(uint32_t value) {
    uint8_t buffer[sizeof(uint16_t) + sizeof(uint32_t)] = {0};
    *((uint32_t*)(buffer + sizeof(uint16_t))) = Endian::host_to_be(value);
    add_option(icmpv6_option(MTU, sizeof(buffer), buffer));
}

void ICMPv6::shortcut_limit(uint8_t value) {
    uint8_t buffer[sizeof(uint16_t) + sizeof(uint32_t)] = {0};
    buffer[0] = value;
    add_option(icmpv6_option(NBMA_SHORT_LIMIT, sizeof(buffer), buffer));
}

void ICMPv6::new_advert_interval(uint32_t value) {
    uint8_t buffer[sizeof(uint16_t) + sizeof(uint32_t)] = {0};
    *((uint32_t*)(buffer + sizeof(uint16_t))) = Endian::host_to_be(value);
    add_option(icmpv6_option(ADVERT_INTERVAL, sizeof(buffer), buffer));
}

void ICMPv6::new_home_agent_info(const new_ha_info_type &value) {
    uint8_t buffer[sizeof(uint16_t) + sizeof(uint32_t)] = {0};
    *((uint16_t*)(buffer + sizeof(uint16_t))) = Endian::host_to_be(value.first);
    *((uint16_t*)(buffer + sizeof(uint16_t) * 2)) = Endian::host_to_be(value.second);
    add_option(icmpv6_option(HOME_AGENT_INFO, sizeof(buffer), buffer));
}

void ICMPv6::source_addr_list(const addr_list_type &value) {
    add_addr_list(S_ADDRESS_LIST, value);
}

void ICMPv6::target_addr_list(const addr_list_type &value) {
    add_addr_list(T_ADDRESS_LIST, value);
}

void ICMPv6::add_addr_list(uint8_t type, const addr_list_type &value) {
    std::vector<uint8_t> buffer;
    buffer.reserve(value.size() + 6);
    buffer.insert(buffer.end(), 6, 0);
    for(addr_list_type::const_iterator it(value.begin()); it != value.end(); ++it)
        buffer.insert(buffer.end(), it->begin(), it->end());
    add_option(icmpv6_option(type, buffer.begin(), buffer.end()));
}

void ICMPv6::rsa_signature(const rsa_sign_type &value) {
    uint32_t total_sz =  4 + sizeof(value.key_hash) + value.signature.size();
    uint8_t padding = 8 - total_sz % 8;
    if(padding == 8)
        padding = 0;
    std::vector<uint8_t> buffer;
    buffer.reserve(total_sz + padding);
    buffer.insert(buffer.end(), 2, 0);
    buffer.insert(buffer.end(), value.key_hash, value.key_hash + sizeof(value.key_hash));
    buffer.insert(buffer.end(), value.signature.begin(), value.signature.end());
    buffer.insert(buffer.end(), padding, 0);
    add_option(icmpv6_option(RSA_SIGN, buffer.begin(), buffer.end()));
}

void ICMPv6::timestamp(uint64_t value) {
    std::vector<uint8_t> buffer(6 + sizeof(uint64_t));
    buffer.insert(buffer.begin(), 6, 0);
    *((uint64_t*)&buffer[6]) = Endian::host_to_be(value);
    add_option(icmpv6_option(TIMESTAMP, buffer.begin(), buffer.end()));
}

void ICMPv6::nonce(const nonce_type &value) {
    add_option(icmpv6_option(NONCE, value.begin(), value.end()));
}

void ICMPv6::ip_prefix(const ip_prefix_type &value) {
    std::vector<uint8_t> buffer;
    buffer.reserve(6 + ipaddress_type::address_size);
    buffer.push_back(value.option_code);
    buffer.push_back(value.prefix_len);
    // reserved
    buffer.insert(buffer.end(), sizeof(uint32_t), 0);
    buffer.insert(buffer.end(), value.address.begin(), value.address.end());
    add_option(icmpv6_option(IP_PREFIX, buffer.begin(), buffer.end()));
}

void ICMPv6::link_layer_addr(lladdr_type value) {
    value.address.insert(value.address.begin(), value.option_code);
    uint8_t padding = 8 - (2 + value.address.size()) % 8;
    if(padding == 8)
        padding = 0;
    value.address.insert(value.address.end(), padding, 0);
    add_option(icmpv6_option(LINK_ADDRESS, value.address.begin(), value.address.end()));
}

void ICMPv6::naack(const naack_type &value) {
    uint8_t buffer[6];
    buffer[0] = value.first;
    buffer[1] = value.second;
    add_option(icmpv6_option(NAACK, buffer, buffer + sizeof(buffer)));
}

void ICMPv6::map(const map_type &value) {
    uint8_t buffer[sizeof(uint8_t) * 2 + sizeof(uint32_t) + ipaddress_type::address_size];
    buffer[0] = value.dist << 4 | value.pref;
    buffer[1] = value.r << 7;
    *(uint32_t*)(buffer + 2) = Endian::host_to_be(value.valid_lifetime);
    value.address.copy(buffer + 2 + sizeof(uint32_t));
    add_option(icmpv6_option(MAP, buffer, buffer + sizeof(buffer)));
}

void ICMPv6::route_info(const route_info_type &value) {
    uint8_t padding = 8 - value.prefix.size() % 8;
    if(padding == 8)
        padding = 0;
    std::vector<uint8_t> buffer(2 + sizeof(uint32_t) + value.prefix.size() + padding);
    buffer[0] = value.prefix_len;
    buffer[1] = value.pref << 3;
    *(uint32_t*)&buffer[2] = Endian::host_to_be(value.route_lifetime);
    // copy the prefix and then fill with padding
    buffer.insert(
        std::copy(value.prefix.begin(), value.prefix.end(), buffer.begin() + 2 + sizeof(uint32_t)),
        padding, 
        0
    );
    add_option(icmpv6_option(ROUTE_INFO, buffer.begin(), buffer.end()));
}

void ICMPv6::recursive_dns_servers(const recursive_dns_type &value) {
    std::vector<uint8_t> buffer(
        2 + sizeof(uint32_t) + value.servers.size() * ipaddress_type::address_size
    );
    
    buffer[0] = buffer[1] = 0;
    *(uint32_t*)&buffer[2] = Endian::host_to_be(value.lifetime);
    std::vector<uint8_t>::iterator out = buffer.begin() + 2 + sizeof(uint32_t);
    typedef recursive_dns_type::servers_type::const_iterator iterator;
    for(iterator it = value.servers.begin(); it != value.servers.end(); ++it)
        out = it->copy(out);
    add_option(icmpv6_option(RECURSIVE_DNS_SERV, buffer.begin(), buffer.end()));
}

void ICMPv6::handover_key_request(const handover_key_req_type &value) {
    uint8_t padding = 8 - (value.key.size() + 4) % 8;
    if(padding == 8)
        padding = 0;
    std::vector<uint8_t> buffer(2 + value.key.size() + padding);
    buffer[0] = padding;
    buffer[1] = value.AT << 4;
    // copy the key, and fill with padding
    std::fill(
        std::copy(value.key.begin(), value.key.end(), buffer.begin() + 2),
        buffer.end(),
        0
    );
    add_option(icmpv6_option(HANDOVER_KEY_REQ, buffer.begin(), buffer.end()));
}

void ICMPv6::handover_key_reply(const handover_key_reply_type &value) {
    const uint32_t data_size = value.key.size() + 2 + sizeof(uint16_t);
    uint8_t padding = 8 - (data_size+2) % 8;
    if(padding == 8)
        padding = 0;
    std::vector<uint8_t> buffer(data_size + padding);
    buffer[0] = padding;
    buffer[1] = value.AT << 4;
    *(uint16_t*)&buffer[2] = Endian::host_to_be(value.lifetime);
    // copy the key, and fill with padding
    std::fill(
        std::copy(value.key.begin(), value.key.end(), buffer.begin() + 2 + sizeof(uint16_t)),
        buffer.end(),
        0
    );
    add_option(icmpv6_option(HANDOVER_KEY_REPLY, buffer.begin(), buffer.end()));
}

void ICMPv6::handover_assist_info(const handover_assist_info_type &value) {
    const uint32_t data_size = value.hai.size() + 2;
    uint8_t padding = 8 - (data_size+2) % 8;
    if(padding == 8)
        padding = 0;
    std::vector<uint8_t> buffer(data_size + padding);
    buffer[0] = value.option_code;
    buffer[1] = static_cast<uint8_t>(value.hai.size());
    // copy hai + padding
    buffer.insert(
        std::copy(value.hai.begin(), value.hai.end(), buffer.begin() + 2),
        padding,
        0
    );
    add_option(icmpv6_option(HANDOVER_ASSIST_INFO, buffer.begin(), buffer.end()));
}

void ICMPv6::mobile_node_identifier(const mobile_node_id_type &value) {
    const uint32_t data_size = value.mn.size() + 2;
    uint8_t padding = 8 - (data_size+2) % 8;
    if(padding == 8)
        padding = 0;
    std::vector<uint8_t> buffer(data_size + padding);
    buffer[0] = value.option_code;
    buffer[1] = static_cast<uint8_t>(value.mn.size());
    // copy mn + padding
    buffer.insert(
        std::copy(value.mn.begin(), value.mn.end(), buffer.begin() + 2),
        padding,
        0
    );
    add_option(icmpv6_option(MOBILE_NODE_ID, buffer.begin(), buffer.end()));
}

void ICMPv6::dns_search_list(const dns_search_list_type &value) {
    // at least it's got this size
    std::vector<uint8_t> buffer(2 + sizeof(uint32_t));
    *(uint32_t*)&buffer[2] = Endian::host_to_be(value.lifetime);
    typedef dns_search_list_type::domains_type::const_iterator iterator;
    for(iterator it = value.domains.begin(); it != value.domains.end(); ++it) {
        size_t prev = 0, index;
        do {
            index = it->find('.', prev);
            std::string::const_iterator end = (index == std::string::npos) ? it->end() : (it->begin() + index);
            buffer.push_back(end - (it->begin() + prev));
            buffer.insert(buffer.end(), it->begin() + prev, end);
            prev = index + 1;
        } while(index != std::string::npos);
        // delimiter
        buffer.push_back(0);
    }
    uint8_t padding = 8 - (buffer.size() + 2) % 8;
    if(padding == 8)
        padding = 0;
    buffer.insert(buffer.end(), padding, 0);
    add_option(icmpv6_option(DNS_SEARCH_LIST, buffer.begin(), buffer.end()));
}

// ********************************************************************
//                          Option getters
// ********************************************************************

ICMPv6::hwaddress_type ICMPv6::source_link_layer_addr() const {
    const icmpv6_option *opt = search_option(SOURCE_ADDRESS);
    if(!opt || opt->data_size() != hwaddress_type::address_size)
        throw option_not_found();
    return hwaddress_type(opt->data_ptr());
}

ICMPv6::hwaddress_type ICMPv6::target_link_layer_addr() const {
    const icmpv6_option *opt = search_option(TARGET_ADDRESS);
    if(!opt || opt->data_size() != hwaddress_type::address_size)
        throw option_not_found();
    return hwaddress_type(opt->data_ptr());
}

ICMPv6::prefix_info_type ICMPv6::prefix_info() const {
    const icmpv6_option *opt = search_option(PREFIX_INFO);
    if(!opt || opt->data_size() != 2 + sizeof(uint32_t) * 3 + ipaddress_type::address_size)
        throw option_not_found();
    const uint8_t *ptr = opt->data_ptr();
    prefix_info_type output;
    output.prefix_len = *ptr++;
    output.L = (*ptr >> 7) & 0x1;
    output.A = (*ptr++ >> 6) & 0x1;
    output.valid_lifetime = Endian::be_to_host(*(uint32_t*)ptr);
    ptr += sizeof(uint32_t);
    output.preferred_lifetime = Endian::be_to_host(*(uint32_t*)ptr);
    output.prefix = ptr + sizeof(uint32_t) * 2;
    return output;
}

PDU::serialization_type ICMPv6::redirect_header() const {
    const icmpv6_option *opt = search_option(REDIRECT_HEADER);
    if(!opt || opt->data_size() < 6)
        throw option_not_found();
    const uint8_t *ptr = opt->data_ptr() + 6;
    return serialization_type(ptr, ptr + opt->data_size() - 6);
}

uint32_t ICMPv6::mtu() const {
    const icmpv6_option *opt = search_option(MTU);
    if(!opt || opt->data_size() != sizeof(uint16_t) + sizeof(uint32_t))
        throw option_not_found();
    return Endian::be_to_host(*(const uint32_t*)(opt->data_ptr() + sizeof(uint16_t)));
}

uint8_t ICMPv6::shortcut_limit() const {
    const icmpv6_option *opt = search_option(NBMA_SHORT_LIMIT);
    if(!opt || opt->data_size() != sizeof(uint16_t) + sizeof(uint32_t))
        throw option_not_found();
    return *opt->data_ptr();
}

uint32_t ICMPv6::new_advert_interval() const {
    const icmpv6_option *opt = search_option(ADVERT_INTERVAL);
    if(!opt || opt->data_size() != sizeof(uint16_t) + sizeof(uint32_t))
        throw option_not_found();
    return Endian::be_to_host(*(const uint32_t*)(opt->data_ptr() + sizeof(uint16_t)));
}

ICMPv6::new_ha_info_type ICMPv6::new_home_agent_info() const {
    const icmpv6_option *opt = search_option(HOME_AGENT_INFO);
    if(!opt || opt->data_size() != sizeof(uint16_t) + sizeof(uint32_t))
        throw option_not_found();
    return std::make_pair(
        Endian::be_to_host(*(const uint16_t*)(opt->data_ptr() + sizeof(uint16_t))),
        Endian::be_to_host(*(const uint16_t*)(opt->data_ptr() + sizeof(uint16_t) * 2))
    );
}

ICMPv6::addr_list_type ICMPv6::source_addr_list() const {
    return search_addr_list(S_ADDRESS_LIST);
}

ICMPv6::addr_list_type ICMPv6::target_addr_list() const {
    return search_addr_list(T_ADDRESS_LIST);
}

ICMPv6::addr_list_type ICMPv6::search_addr_list(Options type) const {
    const icmpv6_option *opt = search_option(type);
    if(!opt || opt->data_size() < 6 + ipaddress_type::address_size)
        throw option_not_found();
    addr_list_type output;
    const uint8_t *ptr = opt->data_ptr() + 6, *end = opt->data_ptr() + opt->data_size();
    while(ptr < end) {
        if(ptr + ipaddress_type::address_size > end)
            throw option_not_found();
        output.push_back(ipaddress_type(ptr));
        ptr += ipaddress_type::address_size;
    }
    return output;
}

ICMPv6::rsa_sign_type ICMPv6::rsa_signature() const {
    const icmpv6_option *opt = search_option(RSA_SIGN);
    // 2 bytes reserved + at least 1 byte signature.
    // 16 == sizeof(rsa_sign_type::key_hash), removed the sizeof
    // expression since gcc 4.2 doesn't like it
    if(!opt || opt->data_size() < 2 + 16 + 1)
        throw option_not_found();
    const uint8_t *ptr = opt->data_ptr() + 2;
    rsa_sign_type output;
    std::copy(ptr, ptr + sizeof(output.key_hash), output.key_hash);
    ptr += sizeof(output.key_hash);
    output.signature.assign(ptr, opt->data_ptr() + opt->data_size());
    return output;
}

uint64_t ICMPv6::timestamp() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        TIMESTAMP, 6 + sizeof(uint64_t)
    );
    return Endian::be_to_host(*(uint64_t*)(opt->data_ptr() + 6));
}

ICMPv6::nonce_type ICMPv6::nonce() const {
    const icmpv6_option *opt = safe_search_option<std::equal_to>(
        NONCE, 0
    );
    return nonce_type(opt->data_ptr(), opt->data_ptr() + opt->data_size());
}

ICMPv6::ip_prefix_type ICMPv6::ip_prefix() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        IP_PREFIX, 2
    );
    const uint8_t *ptr = opt->data_ptr();
    ip_prefix_type output;
    output.option_code = *ptr++;
    output.prefix_len = *ptr++;
    // skip padding
    ptr += sizeof(uint32_t);
    output.address = ipaddress_type(ptr);
    return output;
}   

ICMPv6::lladdr_type ICMPv6::link_layer_addr() const {
    // at least the option_code and 1 byte from the link layer address
    const icmpv6_option *opt = safe_search_option<std::less>(
        LINK_ADDRESS, 2
    );
    const uint8_t *ptr = opt->data_ptr();
    lladdr_type output(*ptr++);
    output.address.assign(ptr, opt->data_ptr() + opt->data_size());
    return output;
}

ICMPv6::naack_type ICMPv6::naack() const {
    const icmpv6_option *opt = safe_search_option<std::not_equal_to>(
        NAACK, 6
    );
    const uint8_t *ptr = opt->data_ptr();
    return naack_type(ptr[0], ptr[1]);
}

ICMPv6::map_type ICMPv6::map() const {
    const icmpv6_option *opt = safe_search_option<std::not_equal_to>(
        MAP, 2 + sizeof(uint32_t) + ipaddress_type::address_size
    );
    const uint8_t *ptr = opt->data_ptr();
    map_type output;
    output.dist = (*ptr >> 4) & 0x0f;
    output.pref = *ptr++ & 0x0f;
    output.r = (*ptr++ >> 7) & 0x01;
    output.valid_lifetime = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);
    output.address = ptr;
    return output;
}

ICMPv6::route_info_type ICMPv6::route_info() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        ROUTE_INFO, 2 + sizeof(uint32_t)
    );
    const uint8_t *ptr = opt->data_ptr();
    route_info_type output;
    output.prefix_len = *ptr++;
    output.pref = (*ptr++ >> 3) & 0x3;
    output.route_lifetime = Endian::be_to_host(*(uint32_t*)ptr);
    ptr += sizeof(uint32_t);
    output.prefix.assign(ptr, opt->data_ptr() + opt->data_size());
    return output;
}

ICMPv6::recursive_dns_type ICMPv6::recursive_dns_servers() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        RECURSIVE_DNS_SERV, 2 + sizeof(uint32_t) + ipaddress_type::address_size
    );
    const uint8_t *ptr = opt->data_ptr() + 2, *end = opt->data_ptr() + opt->data_size();
    recursive_dns_type output;
    output.lifetime = Endian::be_to_host(*(uint32_t*)ptr);
    ptr += sizeof(uint32_t);
    while(ptr < end) {
        if(ptr + ipaddress_type::address_size > end)
            throw option_not_found();
        output.servers.push_back(ptr);
        ptr += ipaddress_type::address_size;
    }
    return output;
}

ICMPv6::handover_key_req_type ICMPv6::handover_key_request() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        HANDOVER_KEY_REQ, 2 + sizeof(uint32_t)
    );
    const uint8_t *ptr = opt->data_ptr() + 1, *end = opt->data_ptr() + opt->data_size();
    handover_key_req_type output;
    output.AT = (*ptr++ >> 4) & 0x3;
    // is there enough size for the indicated padding?
    if(end - ptr < *opt->data_ptr())
        throw option_not_found();
    output.key.assign(ptr, ptr + ((end - ptr) - *opt->data_ptr()));
    return output;
}

ICMPv6::handover_key_reply_type ICMPv6::handover_key_reply() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        HANDOVER_KEY_REPLY, 2 + sizeof(uint32_t)
    );
    const uint8_t *ptr = opt->data_ptr() + 1, *end = opt->data_ptr() + opt->data_size();
    handover_key_reply_type output;
    output.AT = (*ptr++ >> 4) & 0x3;
    output.lifetime = Endian::be_to_host(*(uint16_t*)ptr);
    ptr += sizeof(uint16_t);
    // is there enough size for the indicated padding?
    if(end - ptr < *opt->data_ptr())
        throw option_not_found();
    output.key.assign(ptr, ptr + ((end - ptr) - *opt->data_ptr()));
    return output;
}

ICMPv6::handover_assist_info_type ICMPv6::handover_assist_info() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        HANDOVER_ASSIST_INFO, 2
    );
    const uint8_t *ptr = opt->data_ptr(), *end = ptr + opt->data_size();
    handover_assist_info_type output;
    output.option_code = *ptr++;
    if((end - ptr - 1) < *ptr)
        throw option_not_found();
    output.hai.assign(ptr + 1, ptr + 1 + *ptr);
    return output;
}

ICMPv6::mobile_node_id_type ICMPv6::mobile_node_identifier() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        MOBILE_NODE_ID, 2
    );
    const uint8_t *ptr = opt->data_ptr(), *end = ptr + opt->data_size();
    mobile_node_id_type output;
    output.option_code = *ptr++;
    if((end - ptr - 1) < *ptr)
        throw option_not_found();
    output.mn.assign(ptr + 1, ptr + 1 + *ptr);
    return output;
}

ICMPv6::dns_search_list_type ICMPv6::dns_search_list() const {
    const icmpv6_option *opt = safe_search_option<std::less>(
        DNS_SEARCH_LIST, 2 + sizeof(uint32_t)
    );
    const uint8_t *ptr = opt->data_ptr(), *end = ptr + opt->data_size();
    dns_search_list_type output;
    output.lifetime = Endian::be_to_host(*(uint32_t*)(ptr + 2));
    ptr += 2 + sizeof(uint32_t);
    while(ptr < end && *ptr) {
        std::string domain;
        while(ptr < end && *ptr && *ptr < (end - ptr)) {
            if(!domain.empty())
                domain.push_back('.');
            domain.insert(domain.end(), ptr + 1, ptr + *ptr + 1);
            ptr += *ptr + 1;
        }
        // not enough size
        if(ptr < end && *ptr != 0)
            throw option_not_found();
        output.domains.push_back(domain);
        ptr++;
    }
    return output;
}
}
