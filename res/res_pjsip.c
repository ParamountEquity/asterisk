/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Mark Michelson <mmichelson@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include "asterisk.h"

#include <pjsip.h>
/* Needed for SUBSCRIBE, NOTIFY, and PUBLISH method definitions */
#include <pjsip_simple.h>
#include <pjlib.h>

#include "asterisk/res_pjsip.h"
#include "res_pjsip/include/res_pjsip_private.h"
#include "asterisk/linkedlists.h"
#include "asterisk/logger.h"
#include "asterisk/lock.h"
#include "asterisk/utils.h"
#include "asterisk/astobj2.h"
#include "asterisk/module.h"
#include "asterisk/threadpool.h"
#include "asterisk/taskprocessor.h"
#include "asterisk/uuid.h"
#include "asterisk/sorcery.h"

/*** MODULEINFO
	<depend>pjproject</depend>
	<depend>res_sorcery_config</depend>
	<support_level>core</support_level>
 ***/

/*** DOCUMENTATION
	<configInfo name="res_pjsip" language="en_US">
		<synopsis>SIP Resource using PJProject</synopsis>
		<configFile name="pjsip.conf">
			<configObject name="endpoint">
				<synopsis>Endpoint</synopsis>
				<description><para>
					The <emphasis>Endpoint</emphasis> is the primary configuration object.
					It contains the core SIP related options only, endpoints are <emphasis>NOT</emphasis>
					dialable entries of their own. Communication with another SIP device is
					accomplished via Addresses of Record (AoRs) which have one or more
					contacts assicated with them. Endpoints <emphasis>NOT</emphasis> configured to
					use a <literal>transport</literal> will default to first transport found
					in <filename>pjsip.conf</filename> that matches its type.
					</para>
					<para>Example: An Endpoint has been configured with no transport.
					When it comes time to call an AoR, PJSIP will find the
					first transport that matches the type. A SIP URI of <literal>sip:5000@[11::33]</literal>
					will use the first IPv6 transport and try to send the request.
					</para>
					<para>If the anonymous endpoint identifier is in use an endpoint with the name
					"anonymous@domain" will be searched for as a last resort. If this is not found
					it will fall back to searching for "anonymous". If neither endpoints are found
					the anonymous endpoint identifier will not return an endpoint and anonymous
					calling will not be possible.
					</para>
				</description>
				<configOption name="100rel" default="yes">
					<synopsis>Allow support for RFC3262 provisional ACK tags</synopsis>
					<description>
						<enumlist>
							<enum name="no" />
							<enum name="required" />
							<enum name="yes" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="aggregate_mwi" default="yes">
					<synopsis></synopsis>
					<description><para>When enabled, <replaceable>aggregate_mwi</replaceable> condenses message
					waiting notifications from multiple mailboxes into a single NOTIFY. If it is disabled,
					individual NOTIFYs are sent for each mailbox.</para></description>
				</configOption>
				<configOption name="allow">
					<synopsis>Media Codec(s) to allow</synopsis>
				</configOption>
				<configOption name="aors">
					<synopsis>AoR(s) to be used with the endpoint</synopsis>
					<description><para>
						List of comma separated AoRs that the endpoint should be associated with.
					</para></description>
				</configOption>
				<configOption name="auth">
					<synopsis>Authentication Object(s) associated with the endpoint</synopsis>
					<description><para>
						This is a comma-delimited list of <replaceable>auth</replaceable> sections defined
						in <filename>pjsip.conf</filename> to be used to verify inbound connection attempts.
						</para><para>
						Endpoints without an <literal>authentication</literal> object
						configured will allow connections without vertification.
					</para></description>
				</configOption>
				<configOption name="callerid">
					<synopsis>CallerID information for the endpoint</synopsis>
					<description><para>
						Must be in the format <literal>Name &lt;Number&gt;</literal>,
						or only <literal>&lt;Number&gt;</literal>.
					</para></description>
				</configOption>
				<configOption name="callerid_privacy">
					<synopsis>Default privacy level</synopsis>
					<description>
						<enumlist>
							<enum name="allowed_not_screened" />
							<enum name="allowed_passed_screened" />
							<enum name="allowed_failed_screened" />
							<enum name="allowed" />
							<enum name="prohib_not_screened" />
							<enum name="prohib_passed_screened" />
							<enum name="prohib_failed_screened" />
							<enum name="prohib" />
							<enum name="unavailable" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="callerid_tag">
					<synopsis>Internal id_tag for the endpoint</synopsis>
				</configOption>
				<configOption name="context">
					<synopsis>Dialplan context for inbound sessions</synopsis>
				</configOption>
				<configOption name="direct_media_glare_mitigation" default="none">
					<synopsis>Mitigation of direct media (re)INVITE glare</synopsis>
					<description>
						<para>
						This setting attempts to avoid creating INVITE glare scenarios
						by disabling direct media reINVITEs in one direction thereby allowing
						designated servers (according to this option) to initiate direct
						media reINVITEs without contention and significantly reducing call
						setup time.
						</para>
						<para>
						A more detailed description of how this option functions can be found on
						the Asterisk wiki https://wiki.asterisk.org/wiki/display/AST/SIP+Direct+Media+Reinvite+Glare+Avoidance
						</para>
						<enumlist>
							<enum name="none" />
							<enum name="outgoing" />
							<enum name="incoming" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="direct_media_method" default="invite">
					<synopsis>Direct Media method type</synopsis>
					<description>
						<para>Method for setting up Direct Media between endpoints.</para>
						<enumlist>
							<enum name="invite" />
							<enum name="reinvite">
								<para>Alias for the <literal>invite</literal> value.</para>
							</enum>
							<enum name="update" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="connected_line_method" default="invite">
					<synopsis>Connected line method type</synopsis>
					<description>
						<para>Method used when updating connected line information.</para>
						<enumlist>
							<enum name="invite" />
							<enum name="reinvite">
								<para>Alias for the <literal>invite</literal> value.</para>
							</enum>
							<enum name="update" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="direct_media" default="yes">
					<synopsis>Determines whether media may flow directly between endpoints.</synopsis>
				</configOption>
				<configOption name="disable_direct_media_on_nat" default="no">
					<synopsis>Disable direct media session refreshes when NAT obstructs the media session</synopsis>
				</configOption>
				<configOption name="disallow">
					<synopsis>Media Codec(s) to disallow</synopsis>
				</configOption>
				<configOption name="dtmfmode" default="rfc4733">
					<synopsis>DTMF mode</synopsis>
					<description>
						<para>This setting allows to choose the DTMF mode for endpoint communication.</para>
						<enumlist>
							<enum name="rfc4733">
								<para>DTMF is sent out of band of the main audio stream.This
								supercedes the older <emphasis>RFC-2833</emphasis> used within
								the older <literal>chan_sip</literal>.</para>
							</enum>
							<enum name="inband">
								<para>DTMF is sent as part of audio stream.</para>
							</enum>
							<enum name="info">
								<para>DTMF is sent as SIP INFO packets.</para>
							</enum>
						</enumlist>
					</description>
				</configOption>
				<configOption name="external_media_address">
					<synopsis>IP used for External Media handling</synopsis>
				</configOption>
				<configOption name="force_rport" default="yes">
					<synopsis>Force use of return port</synopsis>
				</configOption>
				<configOption name="ice_support" default="no">
					<synopsis>Enable the ICE mechanism to help traverse NAT</synopsis>
				</configOption>
				<configOption name="identify_by" default="username,location">
					<synopsis>Way(s) for Endpoint to be identified</synopsis>
					<description><para>
						There are currently two methods to identify an endpoint. By default
						both are used to identify an endpoint.
						</para>
						<enumlist>
							<enum name="username" />
							<enum name="location" />
							<enum name="username,location" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="mailboxes">
					<synopsis>Mailbox(es) to be associated with</synopsis>
				</configOption>
				<configOption name="mohsuggest" default="default">
					<synopsis>Default Music On Hold class</synopsis>
				</configOption>
				<configOption name="outbound_auth">
					<synopsis>Authentication object used for outbound requests</synopsis>
				</configOption>
				<configOption name="outbound_proxy">
					<synopsis>Proxy through which to send requests</synopsis>
				</configOption>
				<configOption name="rewrite_contact">
					<synopsis>Allow Contact header to be rewritten with the source IP address-port</synopsis>
				</configOption>
				<configOption name="rtp_ipv6" default="no">
					<synopsis>Allow use of IPv6 for RTP traffic</synopsis>
				</configOption>
				<configOption name="rtp_symmetric" default="no">
					<synopsis>Enforce that RTP must be symmetric</synopsis>
				</configOption>
				<configOption name="send_pai" default="no">
					<synopsis>Send the P-Asserted-Identity header</synopsis>
				</configOption>
				<configOption name="send_rpid" default="no">
					<synopsis>Send the Remote-Party-ID header</synopsis>
				</configOption>
				<configOption name="timers_min_se" default="90">
					<synopsis>Minimum session timers expiration period</synopsis>
					<description><para>
						Minimium session timer expiration period. Time in seconds.
					</para></description>
				</configOption>
				<configOption name="timers" default="yes">
					<synopsis>Session timers for SIP packets</synopsis>
					<description>
						<enumlist>
							<enum name="forced" />
							<enum name="no" />
							<enum name="required" />
							<enum name="yes" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="timers_sess_expires" default="1800">
					<synopsis>Maximum session timer expiration period</synopsis>
					<description><para>
						Maximium session timer expiration period. Time in seconds.
					</para></description>
				</configOption>
				<configOption name="transport">
					<synopsis>Desired transport configuration</synopsis>
					<description><para>
						This will set the desired transport configuration to send SIP data through.
						</para>
						<warning><para>Not specifying a transport will <emphasis>DEFAULT</emphasis>
						to the first configured transport in <filename>pjsip.conf</filename> which is
						valid for the URI we are trying to contact.
						</para></warning>
					</description>
				</configOption>
				<configOption name="trust_id_inbound" default="no">
					<synopsis>Accept identification information received from this endpoint</synopsis>
					<description><para>This option determines whether Asterisk will accept
					identification from the endpoint from headers such as P-Asserted-Identity
					or Remote-Party-ID header. This option applies both to calls originating from the
					endpoint and calls originating from Asterisk. If <literal>no</literal>, the
					configured Caller-ID from pjsip.conf will always be used as the identity for
					the endpoint.</para></description>
				</configOption>
				<configOption name="trust_id_outbound" default="no">
					<synopsis>Send private identification details to the endpoint.</synopsis>
					<description><para>This option determines whether res_pjsip will send private
					identification information to the endpoint. If <literal>no</literal>,
					private Caller-ID information will not be forwarded to the endpoint.
					"Private" in this case refers to any method of restricting identification.
					Example: setting <replaceable>callerid_privacy</replaceable> to any
					<literal>prohib</literal> variation.
					Example: If <replaceable>trust_id_inbound</replaceable> is set to
					<literal>yes</literal>, the presence of a <literal>Privacy: id</literal>
					header in a SIP request or response would indicate the identification
					provided in the request is private.</para></description>
				</configOption>
				<configOption name="type">
					<synopsis>Must be of type 'endpoint'.</synopsis>
				</configOption>
				<configOption name="use_ptime" default="no">
					<synopsis>Use Endpoint's requested packetisation interval</synopsis>
				</configOption>
				<configOption name="use_avpf" default="no">
					<synopsis>Determines whether res_pjsip will use and enforce usage of AVPF for this
					endpoint.</synopsis>
					<description><para>
						If set to <literal>yes</literal>, res_pjsip will use use the AVPF or SAVPF RTP
						profile for all media offers on outbound calls and media updates and will
						decline media offers not using the AVPF or SAVPF profile.
					</para><para>
						If set to <literal>no</literal>, res_pjsip will use use the AVP or SAVP RTP
						profile for all media offers on outbound calls and media updates and will
						decline media offers not using the AVP or SAVP profile.
					</para></description>
				</configOption>
				<configOption name="media_encryption" default="no">
					<synopsis>Determines whether res_pjsip will use and enforce usage of media encryption
					for this endpoint.</synopsis>
					<description>
						<enumlist>
							<enum name="no"><para>
								res_pjsip will offer no encryption and allow no encryption to be setup.
							</para></enum>
							<enum name="sdes"><para>
								res_pjsip will offer standard SRTP setup via in-SDP keys. Encrypted SIP
								transport should be used in conjunction with this option to prevent
								exposure of media encryption keys.
							</para></enum>
							<enum name="dtls"><para>
								res_pjsip will offer DTLS-SRTP setup.
							</para></enum>
						</enumlist>
					</description>
				</configOption>
				<configOption name="inband_progress" default="no">
					<synopsis>Determines whether chan_pjsip will indicate ringing using inband
					    progress.</synopsis>
					<description><para>
						If set to <literal>yes</literal>, chan_pjsip will send a 183 Session Progress
						when told to indicate ringing and will immediately start sending ringing
						as audio.
					</para><para>
						If set to <literal>no</literal>, chan_pjsip will send a 180 Ringing when told
						to indicate ringing and will NOT send it as audio.
					</para></description>
				</configOption>
				<configOption name="callgroup">
					<synopsis>The numeric pickup groups for a channel.</synopsis>
					<description><para>
						Can be set to a comma separated list of numbers or ranges between the values
						of 0-63 (maximum of 64 groups).
					</para></description>
				</configOption>
				<configOption name="pickupgroup">
					<synopsis>The numeric pickup groups that a channel can pickup.</synopsis>
					<description><para>
						Can be set to a comma separated list of numbers or ranges between the values
						of 0-63 (maximum of 64 groups).
					</para></description>
				</configOption>
				<configOption name="namedcallgroup">
					<synopsis>The named pickup groups for a channel.</synopsis>
					<description><para>
						Can be set to a comma separated list of case sensitive strings limited by
						supported line length.
					</para></description>
				</configOption>
				<configOption name="namedpickupgroup">
					<synopsis>The named pickup groups that a channel can pickup.</synopsis>
					<description><para>
						Can be set to a comma separated list of case sensitive strings limited by
						supported line length.
					</para></description>
				</configOption>
				<configOption name="devicestate_busy_at" default="0">
					<synopsis>The number of in-use channels which will cause busy to be returned as device state</synopsis>
					<description><para>
						When the number of in-use channels for the endpoint matches the devicestate_busy_at setting the
						PJSIP channel driver will return busy as the device state instead of in use.
					</para></description>
				</configOption>
				<configOption name="t38udptl" default="no">
					<synopsis>Whether T.38 UDPTL support is enabled or not</synopsis>
					<description><para>
						If set to yes T.38 UDPTL support will be enabled, and T.38 negotiation requests will be accepted
						and relayed.
					</para></description>
				</configOption>
				<configOption name="t38udptl_ec" default="none">
					<synopsis>T.38 UDPTL error correction method</synopsis>
					<description>
						<enumlist>
							<enum name="none"><para>
								No error correction should be used.
							</para></enum>
							<enum name="fec"><para>
								Forward error correction should be used.
							</para></enum>
							<enum name="redundancy"><para>
								Redundacy error correction should be used.
							</para></enum>
						</enumlist>
					</description>
				</configOption>
				<configOption name="t38udptl_maxdatagram" default="0">
					<synopsis>T.38 UDPTL maximum datagram size</synopsis>
					<description><para>
						This option can be set to override the maximum datagram of a remote endpoint for broken
						endpoints.
					</para></description>
				</configOption>
				<configOption name="faxdetect" default="no">
					<synopsis>Whether CNG tone detection is enabled</synopsis>
					<description><para>
						This option can be set to send the session to the fax extension when a CNG tone is
						detected.
					</para></description>
				</configOption>
				<configOption name="t38udptl_nat" default="no">
					<synopsis>Whether NAT support is enabled on UDPTL sessions</synopsis>
					<description><para>
						When enabled the UDPTL stack will send UDPTL packets to the source address of
						received packets.
					</para></description>
				</configOption>
				<configOption name="t38udptl_ipv6" default="no">
					<synopsis>Whether IPv6 is used for UDPTL Sessions</synopsis>
					<description><para>
						When enabled the UDPTL stack will use IPv6.
					</para></description>
				</configOption>
				<configOption name="tonezone">
					<synopsis>Set which country's indications to use for channels created for this endpoint.</synopsis>
				</configOption>
				<configOption name="language">
					<synopsis>Set the default language to use for channels created for this endpoint.</synopsis>
				</configOption>
				<configOption name="one_touch_recording" default="no">
					<synopsis>Determines whether one-touch recording is allowed for this endpoint.</synopsis>
					<see-also>
						<ref type="configOption">recordonfeature</ref>
						<ref type="configOption">recordofffeature</ref>
					</see-also>
				</configOption>
				<configOption name="recordonfeature" default="automixmon">
					<synopsis>The feature to enact when one-touch recording is turned on.</synopsis>
					<description>
						<para>When an INFO request for one-touch recording arrives with a Record header set to "on", this
						feature will be enabled for the channel. The feature designated here can be any built-in
						or dynamic feature defined in features.conf.</para>
						<note><para>This setting has no effect if the endpoint's one_touch_recording option is disabled</para></note>
					</description>
					<see-also>
						<ref type="configOption">one_touch_recording</ref>
						<ref type="configOption">recordofffeature</ref>
					</see-also>
				</configOption>
				<configOption name="recordofffeature" default="automixmon">
					<synopsis>The feature to enact when one-touch recording is turned off.</synopsis>
					<description>
						<para>When an INFO request for one-touch recording arrives with a Record header set to "off", this
						feature will be enabled for the channel. The feature designated here can be any built-in
						or dynamic feature defined in features.conf.</para>
						<note><para>This setting has no effect if the endpoint's one_touch_recording option is disabled</para></note>
					</description>
					<see-also>
						<ref type="configOption">one_touch_recording</ref>
						<ref type="configOption">recordonfeature</ref>
					</see-also>
				</configOption>
				<configOption name="rtpengine" default="asterisk">
					<synopsis>Name of the RTP engine to use for channels created for this endpoint</synopsis>
				</configOption>
				<configOption name="allowtransfer" default="yes">
					<synopsis>Determines whether SIP REFER transfers are allowed for this endpoint</synopsis>
				</configOption>
				<configOption name="sdpowner" default="-">
					<synopsis>String placed as the username portion of an SDP origin (o=) line.</synopsis>
				</configOption>
				<configOption name="sdpsession" default="Asterisk">
					<synopsis>String used for the SDP session (s=) line.</synopsis>
				</configOption>
				<configOption name="tos_audio">
					<synopsis>DSCP TOS bits for audio streams</synopsis>
					<description><para>
						See https://wiki.asterisk.org/wiki/display/AST/IP+Quality+of+Service for more information about QoS settings
					</para></description>
				</configOption>
				<configOption name="tos_video">
					<synopsis>DSCP TOS bits for video streams</synopsis>
					<description><para>
						See https://wiki.asterisk.org/wiki/display/AST/IP+Quality+of+Service for more information about QoS settings
					</para></description>
				</configOption>
				<configOption name="cos_audio">
					<synopsis>Priority for audio streams</synopsis>
					<description><para>
						See https://wiki.asterisk.org/wiki/display/AST/IP+Quality+of+Service for more information about QoS settings
					</para></description>
				</configOption>
				<configOption name="cos_video">
					<synopsis>Priority for video streams</synopsis>
					<description><para>
						See https://wiki.asterisk.org/wiki/display/AST/IP+Quality+of+Service for more information about QoS settings
					</para></description>
				</configOption>
				<configOption name="allowsubscribe" default="yes">
					<synopsis>Determines if endpoint is allowed to initiate subscriptions with Asterisk.</synopsis>
				</configOption>
				<configOption name="subminexpiry" default="60">
					<synopsis>The minimum allowed expiry time for subscriptions initiated by the endpoint.</synopsis>
				</configOption>
				<configOption name="fromuser">
					<synopsis>Username to use in From header for requests to this endpoint.</synopsis>
				</configOption>
				<configOption name="mwifromuser">
					<synopsis>Username to use in From header for unsolicited MWI NOTIFYs to this endpoint.</synopsis>
				</configOption>
				<configOption name="fromdomain">
					<synopsis>Domain to user in From header for requests to this endpoint.</synopsis>
				</configOption>
				<configOption name="dtlsverify">
					<synopsis>Verify that the provided peer certificate is valid</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para></description>
				</configOption>
				<configOption name="dtlsrekey">
					<synopsis>Interval at which to renegotiate the TLS session and rekey the SRTP session</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para><para>
						If this is not set or the value provided is 0 rekeying will be disabled.
					</para></description>
				</configOption>
				<configOption name="dtlscertfile">
					<synopsis>Path to certificate file to present to peer</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para></description>
				</configOption>
				<configOption name="dtlsprivatekey">
					<synopsis>Path to private key for certificate file</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para></description>
				</configOption>
				<configOption name="dtlscipher">
					<synopsis>Cipher to use for DTLS negotiation</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para><para>
						Many options for acceptable ciphers. See link for more:
						http://www.openssl.org/docs/apps/ciphers.html#CIPHER_STRINGS
					</para></description>
				</configOption>
				<configOption name="dtlscafile">
					<synopsis>Path to certificate authority certificate</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para></description>
				</configOption>
				<configOption name="dtlscapath">
					<synopsis>Path to a directory containing certificate authority certificates</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>dtls</literal>.
					</para></description>
				</configOption>
				<configOption name="dtlssetup">
					<synopsis>Whether we are willing to accept connections, connect to the other party, or both.</synopsis>
					<description>
						<para>
							This option only applies if <replaceable>media_encryption</replaceable> is
							set to <literal>dtls</literal>.
						</para>
						<enumlist>
							<enum name="active"><para>
								res_pjsip will make a connection to the peer.
							</para></enum>
							<enum name="passive"><para>
								res_pjsip will accept connections from the peer.
							</para></enum>
							<enum name="actpass"><para>
								res_pjsip will offer and accept connections from the peer.
							</para></enum>
						</enumlist>
					</description>
				</configOption>
				<configOption name="srtp_tag_32">
					<synopsis>Determines whether 32 byte tags should be used instead of 80 byte tags.</synopsis>
					<description><para>
						This option only applies if <replaceable>media_encryption</replaceable> is
						set to <literal>sdes</literal> or <literal>dtls</literal>.
					</para></description>
				</configOption>
			</configObject>
			<configObject name="auth">
				<synopsis>Authentication type</synopsis>
				<description><para>
					Authentication objects hold the authentication information for use
					by other objects such as <literal>endpoints</literal> or <literal>registrations</literal>.
					This also allows for multiple objects to use a single auth object. See
					the <literal>auth_type</literal> config option for password style choices.
				</para></description>
				<configOption name="auth_type" default="userpass">
					<synopsis>Authentication type</synopsis>
					<description><para>
						This option specifies which of the password style config options should be read
						when trying to authenticate an endpoint inbound request. If set to <literal>userpass</literal>
						then we'll read from the 'password' option. For <literal>md5</literal> we'll read
						from 'md5_cred'.
						</para>
						<enumlist>
							<enum name="md5"/>
							<enum name="userpass"/>
						</enumlist>
					</description>
				</configOption>
				<configOption name="nonce_lifetime" default="32">
					<synopsis>Lifetime of a nonce associated with this authentication config.</synopsis>
				</configOption>
				<configOption name="md5_cred">
					<synopsis>MD5 Hash used for authentication.</synopsis>
					<description><para>Only used when auth_type is <literal>md5</literal>.</para></description>
				</configOption>
				<configOption name="password">
					<synopsis>PlainText password used for authentication.</synopsis>
					<description><para>Only used when auth_type is <literal>userpass</literal>.</para></description>
				</configOption>
				<configOption name="realm" default="asterisk">
					<synopsis>SIP realm for endpoint</synopsis>
				</configOption>
				<configOption name="type">
					<synopsis>Must be 'auth'</synopsis>
				</configOption>
				<configOption name="username">
					<synopsis>Username to use for account</synopsis>
				</configOption>
			</configObject>
			<configObject name="nat_hook">
				<synopsis>XXX This exists only to prevent XML documentation errors.</synopsis>
				<configOption name="external_media_address">
					<synopsis>I should be undocumented or hidden</synopsis>
				</configOption>
				<configOption name="method">
					<synopsis>I should be undocumented or hidden</synopsis>
				</configOption>
			</configObject>
			<configObject name="domain_alias">
				<synopsis>Domain Alias</synopsis>
				<description><para>
					Signifies that a domain is an alias. If the domain on a session is
					not found to match an AoR then this object is used to see if we have
					an alias for the AoR to which the endpoint is binding. This objects
					name as defined in configuration should be the domain alias and a 
					config option is provided to specify the domain to be aliased.
				</para></description>
				<configOption name="type">
					<synopsis>Must be of type 'domain_alias'.</synopsis>
				</configOption>
				<configOption name="domain">
					<synopsis>Domain to be aliased</synopsis>
				</configOption>
			</configObject>
			<configObject name="transport">
				<synopsis>SIP Transport</synopsis>
				<description><para>
					<emphasis>Transports</emphasis>
					</para>
					<para>There are different transports and protocol derivatives
						supported by <literal>res_pjsip</literal>. They are in order of
						preference: UDP, TCP, and WebSocket (WS).</para>
					<note><para>Changes to transport configuration in pjsip.conf will only be
						effected on a complete restart of Asterisk. A module reload
						will not suffice.</para></note>
				</description>
				<configOption name="async_operations" default="1">
					<synopsis>Number of simultaneous Asynchronous Operations</synopsis>
				</configOption>
				<configOption name="bind">
					<synopsis>IP Address and optional port to bind to for this transport</synopsis>
				</configOption>
				<configOption name="ca_list_file">
					<synopsis>File containing a list of certificates to read (TLS ONLY)</synopsis>
				</configOption>
				<configOption name="cert_file">
					<synopsis>Certificate file for endpoint (TLS ONLY)</synopsis>
				</configOption>
				<configOption name="cipher">
					<synopsis>Preferred Cryptography Cipher (TLS ONLY)</synopsis>
					<description><para>
						Many options for acceptable ciphers see link for more:
						http://www.openssl.org/docs/apps/ciphers.html#CIPHER_STRINGS
					</para></description>
				</configOption>
				<configOption name="domain">
					<synopsis>Domain the transport comes from</synopsis>
				</configOption>
				<configOption name="external_media_address">
					<synopsis>External Address to use in RTP handling</synopsis>
				</configOption>
				<configOption name="external_signaling_address">
					<synopsis>External address for SIP signalling</synopsis>
				</configOption>
				<configOption name="external_signaling_port" default="0">
					<synopsis>External port for SIP signalling</synopsis>
				</configOption>
				<configOption name="method">
					<synopsis>Method of SSL transport (TLS ONLY)</synopsis>
					<description>
						<enumlist>
							<enum name="default" />
							<enum name="unspecified" />
							<enum name="tlsv1" />
							<enum name="sslv2" />
							<enum name="sslv3" />
							<enum name="sslv23" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="localnet">
					<synopsis>Network to consider local (used for NAT purposes).</synopsis>
					<description><para>This must be in CIDR or dotted decimal format with the IP
					and mask separated with a slash ('/').</para></description>
				</configOption>
				<configOption name="password">
					<synopsis>Password required for transport</synopsis>
				</configOption>
				<configOption name="privkey_file">
					<synopsis>Private key file (TLS ONLY)</synopsis>
				</configOption>
				<configOption name="protocol" default="udp">
					<synopsis>Protocol to use for SIP traffic</synopsis>
					<description>
						<enumlist>
							<enum name="udp" />
							<enum name="tcp" />
							<enum name="tls" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="require_client_cert" default="false">
					<synopsis>Require client certificate (TLS ONLY)</synopsis>
				</configOption>
				<configOption name="type">
					<synopsis>Must be of type 'transport'.</synopsis>
				</configOption>
				<configOption name="verify_client" default="false">
					<synopsis>Require verification of client certificate (TLS ONLY)</synopsis>
				</configOption>
				<configOption name="verify_server" default="false">
					<synopsis>Require verification of server certificate (TLS ONLY)</synopsis>
				</configOption>
			</configObject>
			<configObject name="contact">
				<synopsis>A way of creating an aliased name to a SIP URI</synopsis>
				<description><para>
					Contacts are a way to hide SIP URIs from the dialplan directly.
					They are also used to make a group of contactable parties when
					in use with <literal>AoR</literal> lists.
				</para></description>
				<configOption name="type">
					<synopsis>Must be of type 'contact'.</synopsis>
				</configOption>
				<configOption name="uri">
					<synopsis>SIP URI to contact peer</synopsis>
				</configOption>
				<configOption name="expiration_time">
					<synopsis>Time to keep alive a contact</synopsis>
					<description><para>
						Time to keep alive a contact. String style specification.
					</para></description>
				</configOption>
				<configOption name="qualify_frequency" default="0">
					<synopsis>Interval at which to qualify a contact</synopsis>
					<description><para>
						Interval between attempts to qualify the contact for reachability.
						If <literal>0</literal> never qualify. Time in seconds.
					</para></description>
				</configOption>
			</configObject>
			<configObject name="contact_status">
				<synopsis>Status for a contact</synopsis>
				<description><para>
					The contact status keeps track of whether or not a contact is reachable
					and how long it took to qualify the contact (round trip time).
				</para></description>
				<configOption name="status">
					<synopsis>A contact's status</synopsis>
					<description>
						<enumlist>
							<enum name="AVAILABLE" />
							<enum name="UNAVAILABLE" />
						</enumlist>
					</description>
				</configOption>
				<configOption name="rtt">
					<synopsis>Round trip time</synopsis>
					<description><para>
						The time, in microseconds, it took to qualify the contact.
					</para></description>
				</configOption>
			</configObject>
			<configObject name="aor">
				<synopsis>The configuration for a location of an endpoint</synopsis>
				<description><para>
					An AoR is what allows Asterisk to contact an endpoint via res_pjsip. If no
					AoRs are specified, an endpoint will not be reachable by Asterisk.
					Beyond that, an AoR has other uses within Asterisk, such as inbound
					registration.
					</para><para>
					An <literal>AoR</literal> is a way to allow dialing a group
					of <literal>Contacts</literal> that all use the same
					<literal>endpoint</literal> for calls.
					</para><para>
					This can be used as another way of grouping a list of contacts to dial
					rather than specifing them each directly when dialing via the dialplan.
					This must be used in conjuction with the <literal>PJSIP_DIAL_CONTACTS</literal>.
					</para><para>
					Registrations: For Asterisk to match an inbound registration to an endpoint,
					the AoR object name must match the user portion of the SIP URI in the "To:" 
					header of the inbound SIP registration. That will usually be equivalent
					to the "user name" set in your hard or soft phones configuration.
				</para></description>
				<configOption name="contact">
					<synopsis>Permanent contacts assigned to AoR</synopsis>
					<description><para>
						Contacts specified will be called whenever referenced
						by <literal>chan_pjsip</literal>.
						</para><para>
						Use a separate "contact=" entry for each contact required. Contacts
						are specified using a SIP URI.
					</para></description>
				</configOption>
				<configOption name="default_expiration" default="3600">
					<synopsis>Default expiration time in seconds for contacts that are dynamically bound to an AoR.</synopsis>
				</configOption>
				<configOption name="mailboxes">
					<synopsis>Mailbox(es) to be associated with</synopsis>
					<description><para>This option applies when an external entity subscribes to an AoR
					for message waiting indications. The mailboxes specified will be subscribed to.
					More than one mailbox can be specified with a comma-delimited string.</para></description>
				</configOption>
				<configOption name="maximum_expiration" default="7200">
					<synopsis>Maximum time to keep an AoR</synopsis>
					<description><para>
						Maximium time to keep a peer with explicit expiration. Time in seconds.
					</para></description>
				</configOption>
				<configOption name="max_contacts" default="0">
					<synopsis>Maximum number of contacts that can bind to an AoR</synopsis>
					<description><para>
						Maximum number of contacts that can associate with this AoR. This value does
						not affect the number of contacts that can be added with the "contact" option.
						It only limits contacts added through external interaction, such as
						registration.
						</para>
						<note><para>This should be set to <literal>1</literal> and
						<replaceable>remove_existing</replaceable> set to <literal>yes</literal> if you
						wish to stick with the older <literal>chan_sip</literal> behaviour.
						</para></note>
					</description>
				</configOption>
				<configOption name="minimum_expiration" default="60">
					<synopsis>Minimum keep alive time for an AoR</synopsis>
					<description><para>
						Minimum time to keep a peer with an explict expiration. Time in seconds.
					</para></description>
				</configOption>
				<configOption name="remove_existing" default="no">
					<synopsis>Determines whether new contacts replace existing ones.</synopsis>
					<description><para>
						On receiving a new registration to the AoR should it remove
						the existing contact that was registered against it?
						</para>
						<note><para>This should be set to <literal>yes</literal> and
						<replaceable>max_contacts</replaceable> set to <literal>1</literal> if you
						wish to stick with the older <literal>chan_sip</literal> behaviour.
						</para></note>
					</description>
				</configOption>
				<configOption name="type">
					<synopsis>Must be of type 'aor'.</synopsis>
				</configOption>
				<configOption name="qualify_frequency" default="0">
					<synopsis>Interval at which to qualify an AoR</synopsis>
					<description><para>
						Interval between attempts to qualify the AoR for reachability.
						If <literal>0</literal> never qualify. Time in seconds.
					</para></description>
				</configOption>
				<configOption name="authenticate_qualify" default="no">
					<synopsis>Authenticates a qualify request if needed</synopsis>
					<description><para>
						If true and a qualify request receives a challenge or authenticate response
						authentication is attempted before declaring the contact available.
					</para></description>
				</configOption>
			</configObject>
			<configObject name="system">
				<synopsis>Options that apply to the SIP stack as well as other system-wide settings</synopsis>
				<description><para>
					The settings in this section are global. In addition to being global, the values will
					not be re-evaluated when a reload is performed. This is because the values must be set
					before the SIP stack is initialized. The only way to reset these values is to either 
					restart Asterisk, or unload res_pjsip.so and then load it again.
				</para></description>
				<configOption name="timert1" default="500">
					<synopsis>Set transaction timer T1 value (milliseconds).</synopsis>
					<description><para>
						Timer T1 is the base for determining how long to wait before retransmitting
						requests that receive no response when using an unreliable transport (e.g. UDP).
						For more information on this timer, see RFC 3261, Section 17.1.1.1.
					</para></description>
				</configOption>
				<configOption name="timerb" default="32000">
					<synopsis>Set transaction timer B value (milliseconds).</synopsis>
					<description><para>
						Timer B determines the maximum amount of time to wait after sending an INVITE
						request before terminating the transaction. It is recommended that this be set
						to 64 * Timer T1, but it may be set higher if desired. For more information on
						this timer, see RFC 3261, Section 17.1.1.1.
					</para></description>
				</configOption>
				<configOption name="compactheaders" default="no">
					<synopsis>Use the short forms of common SIP header names.</synopsis>
				</configOption>
				<configOption name="threadpool_initial_size" default="0">
					<synopsis>Initial number of threads in the res_pjsip threadpool.</synopsis>
				</configOption>
				<configOption name="threadpool_auto_increment" default="5">
					<synopsis>The amount by which the number of threads is incremented when necessary.</synopsis>
				</configOption>
				<configOption name="threadpool_idle_timeout" default="60">
					<synopsis>Number of seconds before an idle thread should be disposed of.</synopsis>
				</configOption>
				<configOption name="threadpool_max_size" default="0">
					<synopsis>Maximum number of threads in the res_pjsip threadpool.
					A value of 0 indicates no maximum.</synopsis>
				</configOption>
			</configObject>
			<configObject name="global">
				<synopsis>Options that apply globally to all SIP communications</synopsis>
				<description><para>
					The settings in this section are global. Unlike options in the <literal>system</literal>
					section, these options can be refreshed by performing a reload.
				</para></description>
				<configOption name="maxforwards" default="70">
					<synopsis>Value used in Max-Forwards header for SIP requests.</synopsis>
				</configOption>
				<configOption name="useragent" default="Asterisk &lt;Asterisk Version&gt;">
					<synopsis>Value used in User-Agent header for SIP requests and Server header for SIP responses.</synopsis>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
	<manager name="PJSIPQualify" language="en_US">
		<synopsis>
			Qualify a chan_pjsip endpoint.
		</synopsis>
		<syntax>
			<xi:include xpointer="xpointer(/docs/manager[@name='Login']/syntax/parameter[@name='ActionID'])" />
			<parameter name="Endpoint" required="true">
				<para>The endpoint you want to qualify.</para>
			</parameter>
		</syntax>
		<description>
			<para>Qualify a chan_pjsip endpoint.</para>
		</description>
	</manager>
 ***/


static pjsip_endpoint *ast_pjsip_endpoint;

static struct ast_threadpool *sip_threadpool;

static int register_service(void *data)
{
	pjsip_module **module = data;
	if (!ast_pjsip_endpoint) {
		ast_log(LOG_ERROR, "There is no PJSIP endpoint. Unable to register services\n");
		return -1;
	}
	if (pjsip_endpt_register_module(ast_pjsip_endpoint, *module) != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Unable to register module %.*s\n", (int) pj_strlen(&(*module)->name), pj_strbuf(&(*module)->name));
		return -1;
	}
	ast_debug(1, "Registered SIP service %.*s (%p)\n", (int) pj_strlen(&(*module)->name), pj_strbuf(&(*module)->name), *module);
	ast_module_ref(ast_module_info->self);
	return 0;
}

int ast_sip_register_service(pjsip_module *module)
{
	return ast_sip_push_task_synchronous(NULL, register_service, &module);
}

static int unregister_service(void *data)
{
	pjsip_module **module = data;
	ast_module_unref(ast_module_info->self);
	if (!ast_pjsip_endpoint) {
		return -1;
	}
	pjsip_endpt_unregister_module(ast_pjsip_endpoint, *module);
	ast_debug(1, "Unregistered SIP service %.*s\n", (int) pj_strlen(&(*module)->name), pj_strbuf(&(*module)->name));
	return 0;
}

void ast_sip_unregister_service(pjsip_module *module)
{
	ast_sip_push_task_synchronous(NULL, unregister_service, &module);
}

static struct ast_sip_authenticator *registered_authenticator;

int ast_sip_register_authenticator(struct ast_sip_authenticator *auth)
{
	if (registered_authenticator) {
		ast_log(LOG_WARNING, "Authenticator %p is already registered. Cannot register a new one\n", registered_authenticator);
		return -1;
	}
	registered_authenticator = auth;
	ast_debug(1, "Registered SIP authenticator module %p\n", auth);
	ast_module_ref(ast_module_info->self);
	return 0;
}

void ast_sip_unregister_authenticator(struct ast_sip_authenticator *auth)
{
	if (registered_authenticator != auth) {
		ast_log(LOG_WARNING, "Trying to unregister authenticator %p but authenticator %p registered\n",
				auth, registered_authenticator);
		return;
	}
	registered_authenticator = NULL;
	ast_debug(1, "Unregistered SIP authenticator %p\n", auth);
	ast_module_unref(ast_module_info->self);
}

int ast_sip_requires_authentication(struct ast_sip_endpoint *endpoint, pjsip_rx_data *rdata)
{
	if (!registered_authenticator) {
		ast_log(LOG_WARNING, "No SIP authenticator registered. Assuming authentication is not required\n");
		return 0;
	}

	return registered_authenticator->requires_authentication(endpoint, rdata);
}

enum ast_sip_check_auth_result ast_sip_check_authentication(struct ast_sip_endpoint *endpoint,
		pjsip_rx_data *rdata, pjsip_tx_data *tdata)
{
	if (!registered_authenticator) {
		ast_log(LOG_WARNING, "No SIP authenticator registered. Assuming authentication is successful\n");
		return 0;
	}
	return registered_authenticator->check_authentication(endpoint, rdata, tdata);
}

static struct ast_sip_outbound_authenticator *registered_outbound_authenticator;

int ast_sip_register_outbound_authenticator(struct ast_sip_outbound_authenticator *auth)
{
	if (registered_outbound_authenticator) {
		ast_log(LOG_WARNING, "Outbound authenticator %p is already registered. Cannot register a new one\n", registered_outbound_authenticator);
		return -1;
	}
	registered_outbound_authenticator = auth;
	ast_debug(1, "Registered SIP outbound authenticator module %p\n", auth);
	ast_module_ref(ast_module_info->self);
	return 0;
}

void ast_sip_unregister_outbound_authenticator(struct ast_sip_outbound_authenticator *auth)
{
	if (registered_outbound_authenticator != auth) {
		ast_log(LOG_WARNING, "Trying to unregister outbound authenticator %p but outbound authenticator %p registered\n",
				auth, registered_outbound_authenticator);
		return;
	}
	registered_outbound_authenticator = NULL;
	ast_debug(1, "Unregistered SIP outbound authenticator %p\n", auth);
	ast_module_unref(ast_module_info->self);
}

int ast_sip_create_request_with_auth(const struct ast_sip_auth_array *auths, pjsip_rx_data *challenge,
		pjsip_transaction *tsx, pjsip_tx_data **new_request)
{
	if (!registered_outbound_authenticator) {
		ast_log(LOG_WARNING, "No SIP outbound authenticator registered. Cannot respond to authentication challenge\n");
		return -1;
	}
	return registered_outbound_authenticator->create_request_with_auth(auths, challenge, tsx, new_request);
}

struct endpoint_identifier_list {
	struct ast_sip_endpoint_identifier *identifier;
	AST_RWLIST_ENTRY(endpoint_identifier_list) list;
};

static AST_RWLIST_HEAD_STATIC(endpoint_identifiers, endpoint_identifier_list);

int ast_sip_register_endpoint_identifier(struct ast_sip_endpoint_identifier *identifier)
{
	struct endpoint_identifier_list *id_list_item;
	SCOPED_LOCK(lock, &endpoint_identifiers, AST_RWLIST_WRLOCK, AST_RWLIST_UNLOCK);

	id_list_item = ast_calloc(1, sizeof(*id_list_item));
	if (!id_list_item) {
		ast_log(LOG_ERROR, "Unabled to add endpoint identifier. Out of memory.\n");
		return -1;
	}
	id_list_item->identifier = identifier;

	AST_RWLIST_INSERT_TAIL(&endpoint_identifiers, id_list_item, list);
	ast_debug(1, "Registered endpoint identifier %p\n", identifier);

	ast_module_ref(ast_module_info->self);
	return 0;
}

void ast_sip_unregister_endpoint_identifier(struct ast_sip_endpoint_identifier *identifier)
{
	struct endpoint_identifier_list *iter;
	SCOPED_LOCK(lock, &endpoint_identifiers, AST_RWLIST_WRLOCK, AST_RWLIST_UNLOCK);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&endpoint_identifiers, iter, list) {
		if (iter->identifier == identifier) {
			AST_RWLIST_REMOVE_CURRENT(list);
			ast_free(iter);
			ast_debug(1, "Unregistered endpoint identifier %p\n", identifier);
			ast_module_unref(ast_module_info->self);
			break;
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
}

struct ast_sip_endpoint *ast_sip_identify_endpoint(pjsip_rx_data *rdata)
{
	struct endpoint_identifier_list *iter;
	struct ast_sip_endpoint *endpoint = NULL;
	SCOPED_LOCK(lock, &endpoint_identifiers, AST_RWLIST_RDLOCK, AST_RWLIST_UNLOCK);
	AST_RWLIST_TRAVERSE(&endpoint_identifiers, iter, list) {
		ast_assert(iter->identifier->identify_endpoint != NULL);
		endpoint = iter->identifier->identify_endpoint(rdata);
		if (endpoint) {
			break;
		}
	}
	return endpoint;
}

pjsip_endpoint *ast_sip_get_pjsip_endpoint(void)
{
	return ast_pjsip_endpoint;
}

static int sip_dialog_create_from(pj_pool_t *pool, pj_str_t *from, const char *user, const char *domain, const pj_str_t *target, pjsip_tpselector *selector)
{
	pj_str_t tmp, local_addr;
	pjsip_uri *uri;
	pjsip_sip_uri *sip_uri;
	pjsip_transport_type_e type = PJSIP_TRANSPORT_UNSPECIFIED;
	int local_port;
	char uuid_str[AST_UUID_STR_LEN];

	if (ast_strlen_zero(user)) {
		RAII_VAR(struct ast_uuid *, uuid, ast_uuid_generate(), ast_free_ptr);
		if (!uuid) {
			return -1;
		}
		user = ast_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
	}

	/* Parse the provided target URI so we can determine what transport it will end up using */
	pj_strdup_with_null(pool, &tmp, target);

	if (!(uri = pjsip_parse_uri(pool, tmp.ptr, tmp.slen, 0)) ||
	    (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))) {
		return -1;
	}

	sip_uri = pjsip_uri_get_uri(uri);

	/* Determine the transport type to use */
	if (PJSIP_URI_SCHEME_IS_SIPS(sip_uri)) {
		type = PJSIP_TRANSPORT_TLS;
	} else if (!sip_uri->transport_param.slen) {
		type = PJSIP_TRANSPORT_UDP;
	} else {
		type = pjsip_transport_get_type_from_name(&sip_uri->transport_param);
	}

	if (type == PJSIP_TRANSPORT_UNSPECIFIED) {
		return -1;
	}

	/* If the host is IPv6 turn the transport into an IPv6 version */
	if (pj_strchr(&sip_uri->host, ':') && type < PJSIP_TRANSPORT_START_OTHER) {
		type = (pjsip_transport_type_e)(((int)type) + PJSIP_TRANSPORT_IPV6);
	}

	if (!ast_strlen_zero(domain)) {
		from->ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
		from->slen = pj_ansi_snprintf(from->ptr, PJSIP_MAX_URL_SIZE,
				"<%s:%s@%s%s%s>",
				(pjsip_transport_get_flag_from_type(type) & PJSIP_TRANSPORT_SECURE) ? "sips" : "sip",
				user,
				domain,
				(type != PJSIP_TRANSPORT_UDP && type != PJSIP_TRANSPORT_UDP6) ? ";transport=" : "",
				(type != PJSIP_TRANSPORT_UDP && type != PJSIP_TRANSPORT_UDP6) ? pjsip_transport_get_type_name(type) : "");
		return 0;
	}

	/* Get the local bound address for the transport that will be used when communicating with the provided URI */
	if (pjsip_tpmgr_find_local_addr(pjsip_endpt_get_tpmgr(ast_sip_get_pjsip_endpoint()), pool, type, selector,
							      &local_addr, &local_port) != PJ_SUCCESS) {
		return -1;
	}

	/* If IPv6 was specified in the transport, set the proper type */
	if (pj_strchr(&local_addr, ':') && type < PJSIP_TRANSPORT_START_OTHER) {
		type = (pjsip_transport_type_e)(((int)type) + PJSIP_TRANSPORT_IPV6);
	}

	from->ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
	from->slen = pj_ansi_snprintf(from->ptr, PJSIP_MAX_URL_SIZE,
				      "<%s:%s@%s%.*s%s:%d%s%s>",
				      (pjsip_transport_get_flag_from_type(type) & PJSIP_TRANSPORT_SECURE) ? "sips" : "sip",
				      user,
				      (type & PJSIP_TRANSPORT_IPV6) ? "[" : "",
				      (int)local_addr.slen,
				      local_addr.ptr,
				      (type & PJSIP_TRANSPORT_IPV6) ? "]" : "",
				      local_port,
				      (type != PJSIP_TRANSPORT_UDP && type != PJSIP_TRANSPORT_UDP6) ? ";transport=" : "",
				      (type != PJSIP_TRANSPORT_UDP && type != PJSIP_TRANSPORT_UDP6) ? pjsip_transport_get_type_name(type) : "");

	return 0;
}

static int sip_get_tpselector_from_endpoint(const struct ast_sip_endpoint *endpoint, pjsip_tpselector *selector)
{
	RAII_VAR(struct ast_sip_transport *, transport, NULL, ao2_cleanup);
	const char *transport_name = endpoint->transport;

	if (ast_strlen_zero(transport_name)) {
		return 0;
	}

	transport = ast_sorcery_retrieve_by_id(ast_sip_get_sorcery(), "transport", transport_name);

	if (!transport || !transport->state) {
		return -1;
	}

	if (transport->state->transport) {
		selector->type = PJSIP_TPSELECTOR_TRANSPORT;
		selector->u.transport = transport->state->transport;
	} else if (transport->state->factory) {
		selector->type = PJSIP_TPSELECTOR_LISTENER;
		selector->u.listener = transport->state->factory;
	} else {
		return -1;
	}

	return 0;
}

static int sip_get_tpselector_from_uri(const char *uri, pjsip_tpselector *selector)
{
	RAII_VAR(struct ast_sip_contact_transport *, contact_transport, NULL, ao2_cleanup);

	contact_transport = ast_sip_location_retrieve_contact_transport_by_uri(uri);

	if (!contact_transport) {
		return -1;
	}

	selector->type = PJSIP_TPSELECTOR_TRANSPORT;
	selector->u.transport = contact_transport->transport;

	return 0;
}

pjsip_dialog *ast_sip_create_dialog(const struct ast_sip_endpoint *endpoint, const char *uri, const char *request_user)
{
	pj_str_t local_uri = { "sip:temp@temp", 13 }, remote_uri;
	pjsip_dialog *dlg = NULL;
	const char *outbound_proxy = endpoint->outbound_proxy;
	pjsip_tpselector selector = { .type = PJSIP_TPSELECTOR_NONE, };
	static const pj_str_t HCONTACT = { "Contact", 7 };

	pj_cstr(&remote_uri, uri);

	if (pjsip_dlg_create_uac(pjsip_ua_instance(), &local_uri, NULL, &remote_uri, NULL, &dlg) != PJ_SUCCESS) {
		return NULL;
	}

	if (sip_get_tpselector_from_uri(uri, &selector) && sip_get_tpselector_from_endpoint(endpoint, &selector)) {
		pjsip_dlg_terminate(dlg);
		return NULL;
	}

	if (sip_dialog_create_from(dlg->pool, &local_uri, endpoint->fromuser, endpoint->fromdomain, &remote_uri, &selector)) {
		pjsip_dlg_terminate(dlg);
		return NULL;
	}

	/* Update the dialog with the new local URI, we do it afterwards so we can use the dialog pool for construction */
	pj_strdup_with_null(dlg->pool, &dlg->local.info_str, &local_uri);
	dlg->local.info->uri = pjsip_parse_uri(dlg->pool, dlg->local.info_str.ptr, dlg->local.info_str.slen, 0);
	dlg->local.contact = pjsip_parse_hdr(dlg->pool, &HCONTACT, local_uri.ptr, local_uri.slen, NULL);

	/* If a request user has been specified and we are permitted to change it, do so */
	if (!ast_strlen_zero(request_user) && (PJSIP_URI_SCHEME_IS_SIP(dlg->target) || PJSIP_URI_SCHEME_IS_SIPS(dlg->target))) {
		pjsip_sip_uri *target = pjsip_uri_get_uri(dlg->target);
		pj_strdup2(dlg->pool, &target->user, request_user);
	}

	/* We have to temporarily bump up the sess_count here so the dialog is not prematurely destroyed */
	dlg->sess_count++;

	pjsip_dlg_set_transport(dlg, &selector);

	if (!ast_strlen_zero(outbound_proxy)) {
		pjsip_route_hdr route_set, *route;
		static const pj_str_t ROUTE_HNAME = { "Route", 5 };
		pj_str_t tmp;

		pj_list_init(&route_set);

		pj_strdup2_with_null(dlg->pool, &tmp, outbound_proxy);
		if (!(route = pjsip_parse_hdr(dlg->pool, &ROUTE_HNAME, tmp.ptr, tmp.slen, NULL))) {
			pjsip_dlg_terminate(dlg);
			return NULL;
		}
		pj_list_push_back(&route_set, route);

		pjsip_dlg_set_route_set(dlg, &route_set);
	}

	dlg->sess_count--;

	return dlg;
}

/* PJSIP doesn't know about the INFO method, so we have to define it ourselves */
const pjsip_method pjsip_info_method = {PJSIP_OTHER_METHOD, {"INFO", 4} };
const pjsip_method pjsip_message_method = {PJSIP_OTHER_METHOD, {"MESSAGE", 7} };

static struct {
	const char *method;
	const pjsip_method *pmethod;
} methods [] = {
	{ "INVITE", &pjsip_invite_method },
	{ "CANCEL", &pjsip_cancel_method },
	{ "ACK", &pjsip_ack_method },
	{ "BYE", &pjsip_bye_method },
	{ "REGISTER", &pjsip_register_method },
	{ "OPTIONS", &pjsip_options_method },
	{ "SUBSCRIBE", &pjsip_subscribe_method },
	{ "NOTIFY", &pjsip_notify_method },
	{ "PUBLISH", &pjsip_publish_method },
	{ "INFO", &pjsip_info_method },
	{ "MESSAGE", &pjsip_message_method },
};

static const pjsip_method *get_pjsip_method(const char *method)
{
	int i;
	for (i = 0; i < ARRAY_LEN(methods); ++i) {
		if (!strcmp(method, methods[i].method)) {
			return methods[i].pmethod;
		}
	}
	return NULL;
}

static int create_in_dialog_request(const pjsip_method *method, struct pjsip_dialog *dlg, pjsip_tx_data **tdata)
{
	if (pjsip_dlg_create_request(dlg, method, -1, tdata) != PJ_SUCCESS) {
		ast_log(LOG_WARNING, "Unable to create in-dialog request.\n");
		return -1;
	}

	return 0;
}

static int create_out_of_dialog_request(const pjsip_method *method, struct ast_sip_endpoint *endpoint,
		const char *uri, pjsip_tx_data **tdata)
{
	RAII_VAR(struct ast_sip_contact *, contact, NULL, ao2_cleanup);
	pj_str_t remote_uri;
	pj_str_t from;
	pj_pool_t *pool;
	pjsip_tpselector selector = { .type = PJSIP_TPSELECTOR_NONE, };

	if (ast_strlen_zero(uri)) {
		if (!endpoint) {
			ast_log(LOG_ERROR, "An endpoint and/or uri must be specified\n");
			return -1;
		}

		contact = ast_sip_location_retrieve_contact_from_aor_list(endpoint->aors);
		if (!contact || ast_strlen_zero(contact->uri)) {
			ast_log(LOG_ERROR, "Unable to retrieve contact for endpoint %s\n",
					ast_sorcery_object_get_id(endpoint));
			return -1;
		}

		pj_cstr(&remote_uri, contact->uri);
	} else {
		pj_cstr(&remote_uri, uri);
	}

	if (endpoint) {
		if (sip_get_tpselector_from_endpoint(endpoint, &selector)) {
			ast_log(LOG_ERROR, "Unable to retrieve PJSIP transport selector for endpoint %s\n",
				ast_sorcery_object_get_id(endpoint));
			return -1;
		}
	}

	pool = pjsip_endpt_create_pool(ast_sip_get_pjsip_endpoint(), "Outbound request", 256, 256);

	if (!pool) {
		ast_log(LOG_ERROR, "Unable to create PJLIB memory pool\n");
		return -1;
	}

	if (sip_dialog_create_from(pool, &from, endpoint ? endpoint->fromuser : NULL,
				endpoint ? endpoint->fromdomain : NULL, &remote_uri, &selector)) {
		ast_log(LOG_ERROR, "Unable to create From header for %.*s request to endpoint %s\n",
				(int) pj_strlen(&method->name), pj_strbuf(&method->name), ast_sorcery_object_get_id(endpoint));
		pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
		return -1;
	}

	if (pjsip_endpt_create_request(ast_sip_get_pjsip_endpoint(), method, &remote_uri,
			&from, &remote_uri, &from, NULL, -1, NULL, tdata) != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Unable to create outbound %.*s request to endpoint %s\n",
				(int) pj_strlen(&method->name), pj_strbuf(&method->name), ast_sorcery_object_get_id(endpoint));
		pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
		return -1;
	}

	/* We can release this pool since request creation copied all the necessary
	 * data into the outbound request's pool
	 */
	pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
	return 0;
}

int ast_sip_create_request(const char *method, struct pjsip_dialog *dlg,
		struct ast_sip_endpoint *endpoint, const char *uri,
		pjsip_tx_data **tdata)
{
	const pjsip_method *pmethod = get_pjsip_method(method);

	if (!pmethod) {
		ast_log(LOG_WARNING, "Unknown method '%s'. Cannot send request\n", method);
		return -1;
	}

	if (dlg) {
		return create_in_dialog_request(pmethod, dlg, tdata);
	} else {
		return create_out_of_dialog_request(pmethod, endpoint, uri, tdata);
	}
}

static int send_in_dialog_request(pjsip_tx_data *tdata, struct pjsip_dialog *dlg)
{
	if (pjsip_dlg_send_request(dlg, tdata, -1, NULL) != PJ_SUCCESS) {
		ast_log(LOG_WARNING, "Unable to send in-dialog request.\n");
		return -1;
	}
	return 0;
}

static void send_request_cb(void *token, pjsip_event *e)
{
	RAII_VAR(struct ast_sip_endpoint *, endpoint, token, ao2_cleanup);
	pjsip_transaction *tsx = e->body.tsx_state.tsx;
	pjsip_rx_data *challenge = e->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;

	if (tsx->status_code != 401 && tsx->status_code != 407) {
		return;
	}

	if (!ast_sip_create_request_with_auth(&endpoint->outbound_auths, challenge, tsx, &tdata)) {
		pjsip_endpt_send_request(ast_sip_get_pjsip_endpoint(), tdata, -1, NULL, NULL);
	}
}

static int send_out_of_dialog_request(pjsip_tx_data *tdata, struct ast_sip_endpoint *endpoint)
{
	ao2_ref(endpoint, +1);
	if (pjsip_endpt_send_request(ast_sip_get_pjsip_endpoint(), tdata, -1, endpoint, send_request_cb) != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Error attempting to send outbound %.*s request to endpoint %s\n",
				(int) pj_strlen(&tdata->msg->line.req.method.name),
				pj_strbuf(&tdata->msg->line.req.method.name),
				ast_sorcery_object_get_id(endpoint));
		ao2_ref(endpoint, -1);
		return -1;
	}

	return 0;
}

int ast_sip_send_request(pjsip_tx_data *tdata, struct pjsip_dialog *dlg, struct ast_sip_endpoint *endpoint)
{
	ast_assert(tdata->msg->type == PJSIP_REQUEST_MSG);

	if (dlg) {
		return send_in_dialog_request(tdata, dlg);
	} else {
		return send_out_of_dialog_request(tdata, endpoint);
	}
}

int ast_sip_add_header(pjsip_tx_data *tdata, const char *name, const char *value)
{
	pj_str_t hdr_name;
	pj_str_t hdr_value;
	pjsip_generic_string_hdr *hdr;

	pj_cstr(&hdr_name, name);
	pj_cstr(&hdr_value, value);

	hdr = pjsip_generic_string_hdr_create(tdata->pool, &hdr_name, &hdr_value);

	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr *) hdr);
	return 0;
}

static pjsip_msg_body *ast_body_to_pjsip_body(pj_pool_t *pool, const struct ast_sip_body *body)
{
	pj_str_t type;
	pj_str_t subtype;
	pj_str_t body_text;

	pj_cstr(&type, body->type);
	pj_cstr(&subtype, body->subtype);
	pj_cstr(&body_text, body->body_text);

	return pjsip_msg_body_create(pool, &type, &subtype, &body_text);
}

int ast_sip_add_body(pjsip_tx_data *tdata, const struct ast_sip_body *body)
{
	pjsip_msg_body *pjsip_body = ast_body_to_pjsip_body(tdata->pool, body);
	tdata->msg->body = pjsip_body;
	return 0;
}

int ast_sip_add_body_multipart(pjsip_tx_data *tdata, const struct ast_sip_body *bodies[], int num_bodies)
{
	int i;
	/* NULL for type and subtype automatically creates "multipart/mixed" */
	pjsip_msg_body *body = pjsip_multipart_create(tdata->pool, NULL, NULL);

	for (i = 0; i < num_bodies; ++i) {
		pjsip_multipart_part *part = pjsip_multipart_create_part(tdata->pool);
		part->body = ast_body_to_pjsip_body(tdata->pool, bodies[i]);
		pjsip_multipart_add_part(tdata->pool, body, part);
	}

	tdata->msg->body = body;
	return 0;
}

int ast_sip_append_body(pjsip_tx_data *tdata, const char *body_text)
{
	size_t combined_size = strlen(body_text) + tdata->msg->body->len;
	struct ast_str *body_buffer = ast_str_alloca(combined_size);

	ast_str_set(&body_buffer, 0, "%.*s%s", (int) tdata->msg->body->len, (char *) tdata->msg->body->data, body_text);

	tdata->msg->body->data = pj_pool_alloc(tdata->pool, combined_size);
	pj_memcpy(tdata->msg->body->data, ast_str_buffer(body_buffer), combined_size);
	tdata->msg->body->len = combined_size;

	return 0;
}

struct ast_taskprocessor *ast_sip_create_serializer(void)
{
	struct ast_taskprocessor *serializer;
	RAII_VAR(struct ast_uuid *, uuid, ast_uuid_generate(), ast_free_ptr);
	char name[AST_UUID_STR_LEN];

	if (!uuid) {
		return NULL;
	}

	ast_uuid_to_str(uuid, name, sizeof(name));

	serializer = ast_threadpool_serializer(name, sip_threadpool);
	if (!serializer) {
		return NULL;
	}
	return serializer;
}

int ast_sip_push_task(struct ast_taskprocessor *serializer, int (*sip_task)(void *), void *task_data)
{
	if (serializer) {
		return ast_taskprocessor_push(serializer, sip_task, task_data);
	} else {
		return ast_threadpool_push(sip_threadpool, sip_task, task_data);
	}
}

struct sync_task_data {
	ast_mutex_t lock;
	ast_cond_t cond;
	int complete;
	int fail;
	int (*task)(void *);
	void *task_data;
};

static int sync_task(void *data)
{
	struct sync_task_data *std = data;
	std->fail = std->task(std->task_data);

	ast_mutex_lock(&std->lock);
	std->complete = 1;
	ast_cond_signal(&std->cond);
	ast_mutex_unlock(&std->lock);
	return std->fail;
}

int ast_sip_push_task_synchronous(struct ast_taskprocessor *serializer, int (*sip_task)(void *), void *task_data)
{
	/* This method is an onion */
	struct sync_task_data std;
	ast_mutex_init(&std.lock);
	ast_cond_init(&std.cond, NULL);
	std.fail = std.complete = 0;
	std.task = sip_task;
	std.task_data = task_data;

	if (serializer) {
		if (ast_taskprocessor_push(serializer, sync_task, &std)) {
			return -1;
		}
	} else {
		if (ast_threadpool_push(sip_threadpool, sync_task, &std)) {
			return -1;
		}
	}

	ast_mutex_lock(&std.lock);
	while (!std.complete) {
		ast_cond_wait(&std.cond, &std.lock);
	}
	ast_mutex_unlock(&std.lock);

	ast_mutex_destroy(&std.lock);
	ast_cond_destroy(&std.cond);
	return std.fail;
}

void ast_copy_pj_str(char *dest, const pj_str_t *src, size_t size)
{
	size_t chars_to_copy = MIN(size - 1, pj_strlen(src));
	memcpy(dest, pj_strbuf(src), chars_to_copy);
	dest[chars_to_copy] = '\0';
}

int ast_sip_is_content_type(pjsip_media_type *content_type, char *type, char *subtype)
{
	pjsip_media_type compare;

	if (!content_type) {
		return 0;
	}

	pjsip_media_type_init2(&compare, type, subtype);

	return pjsip_media_type_cmp(content_type, &compare, 0) ? -1 : 0;
}

pj_caching_pool caching_pool;
pj_pool_t *memory_pool;
pj_thread_t *monitor_thread;
static int monitor_continue;

static void *monitor_thread_exec(void *endpt)
{
	while (monitor_continue) {
		const pj_time_val delay = {0, 10};
		pjsip_endpt_handle_events(ast_pjsip_endpoint, &delay);
	}
	return NULL;
}

static void stop_monitor_thread(void)
{
	monitor_continue = 0;
	pj_thread_join(monitor_thread);
}

AST_THREADSTORAGE(pj_thread_storage);
AST_THREADSTORAGE(servant_id_storage);
#define SIP_SERVANT_ID 0x5E2F1D

static void sip_thread_start(void)
{
	pj_thread_desc *desc;
	pj_thread_t *thread;
	uint32_t *servant_id;

	servant_id = ast_threadstorage_get(&servant_id_storage, sizeof(*servant_id));
	if (!servant_id) {
		ast_log(LOG_ERROR, "Could not set SIP servant ID in thread-local storage.\n");
		return;
	}
	*servant_id = SIP_SERVANT_ID;

	desc = ast_threadstorage_get(&pj_thread_storage, sizeof(pj_thread_desc));
	if (!desc) {
		ast_log(LOG_ERROR, "Could not get thread desc from thread-local storage. Expect awful things to occur\n");
		return;
	}
	pj_bzero(*desc, sizeof(*desc));

	if (pj_thread_register("Asterisk Thread", *desc, &thread) != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Couldn't register thread with PJLIB.\n");
	}
}

int ast_sip_thread_is_servant(void)
{
	uint32_t *servant_id;

	servant_id = ast_threadstorage_get(&servant_id_storage, sizeof(*servant_id));
	if (!servant_id) {
		return 0;
	}

	return *servant_id == SIP_SERVANT_ID;
}

static void remove_request_headers(pjsip_endpoint *endpt)
{
	const pjsip_hdr *request_headers = pjsip_endpt_get_request_headers(endpt);
	pjsip_hdr *iter = request_headers->next;

	while (iter != request_headers) {
		pjsip_hdr *to_erase = iter;
		iter = iter->next;
		pj_list_erase(to_erase);
	}
}

static int load_module(void)
{
	/* The third parameter is just copied from
	 * example code from PJLIB. This can be adjusted
	 * if necessary.
	 */
	pj_status_t status;
	struct ast_threadpool_options options;

	if (pj_init() != PJ_SUCCESS) {
		return AST_MODULE_LOAD_DECLINE;
	}

	if (pjlib_util_init() != PJ_SUCCESS) {
		pj_shutdown();
		return AST_MODULE_LOAD_DECLINE;
	}

	pj_caching_pool_init(&caching_pool, NULL, 1024 * 1024);
	if (pjsip_endpt_create(&caching_pool.factory, "SIP", &ast_pjsip_endpoint) != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Failed to create PJSIP endpoint structure. Aborting load\n");
		goto error;
	}

	/* PJSIP will automatically try to add a Max-Forwards header. Since we want to control that,
	 * we need to stop PJSIP from doing it automatically
	 */
	remove_request_headers(ast_pjsip_endpoint);

	memory_pool = pj_pool_create(&caching_pool.factory, "SIP", 1024, 1024, NULL);
	if (!memory_pool) {
		ast_log(LOG_ERROR, "Failed to create memory pool for SIP. Aborting load\n");
		goto error;
	}

	if (ast_sip_initialize_system()) {
		ast_log(LOG_ERROR, "Failed to initialize SIP system configuration. Aborting load\n");
		goto error;
	}

	sip_get_threadpool_options(&options);
	options.thread_start = sip_thread_start;
	sip_threadpool = ast_threadpool_create("SIP", NULL, &options);
	if (!sip_threadpool) {
		ast_log(LOG_ERROR, "Failed to create SIP threadpool. Aborting load\n");
		goto error;
	}

	pjsip_tsx_layer_init_module(ast_pjsip_endpoint);
	pjsip_ua_init_module(ast_pjsip_endpoint, NULL);

	monitor_continue = 1;
	status = pj_thread_create(memory_pool, "SIP", (pj_thread_proc *) &monitor_thread_exec,
			NULL, PJ_THREAD_DEFAULT_STACK_SIZE * 2, 0, &monitor_thread);
	if (status != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Failed to start SIP monitor thread. Aborting load\n");
		goto error;
	}

	ast_sip_initialize_global_headers();

	if (ast_res_pjsip_initialize_configuration()) {
		ast_log(LOG_ERROR, "Failed to initialize SIP configuration. Aborting load\n");
		goto error;
	}

	if (ast_sip_initialize_distributor()) {
		ast_log(LOG_ERROR, "Failed to register distributor module. Aborting load\n");
		goto error;
	}

	if (ast_sip_initialize_outbound_authentication()) {
		ast_log(LOG_ERROR, "Failed to initialize outbound authentication. Aborting load\n");
		goto error;
	}

	ast_res_pjsip_init_options_handling(0);

	ast_res_pjsip_init_contact_transports();

return AST_MODULE_LOAD_SUCCESS;

error:
	ast_sip_destroy_distributor();
	ast_res_pjsip_destroy_configuration();
	ast_sip_destroy_global_headers();
	if (monitor_thread) {
		stop_monitor_thread();
	}
	if (memory_pool) {
		pj_pool_release(memory_pool);
		memory_pool = NULL;
	}
	if (ast_pjsip_endpoint) {
		pjsip_endpt_destroy(ast_pjsip_endpoint);
		ast_pjsip_endpoint = NULL;
	}
	pj_caching_pool_destroy(&caching_pool);
	return AST_MODULE_LOAD_DECLINE;
}

static int reload_module(void)
{
	if (ast_res_pjsip_reload_configuration()) {
		return AST_MODULE_LOAD_DECLINE;
	}
	ast_res_pjsip_init_options_handling(1);
	return 0;
}

static int unload_pjsip(void *data)
{
	if (memory_pool) {
		pj_pool_release(memory_pool);
		memory_pool = NULL;
	}
	if (ast_pjsip_endpoint) {
		pjsip_endpt_destroy(ast_pjsip_endpoint);
		ast_pjsip_endpoint = NULL;
	}
	pj_caching_pool_destroy(&caching_pool);
	return 0;
}

static int unload_module(void)
{
	ast_res_pjsip_cleanup_options_handling();
	ast_sip_destroy_distributor();
	ast_res_pjsip_destroy_configuration();
	ast_sip_destroy_global_headers();
	if (monitor_thread) {
		stop_monitor_thread();
	}
	/* The thread this is called from cannot call PJSIP/PJLIB functions,
	 * so we have to push the work to the threadpool to handle
	 */
	ast_sip_push_task_synchronous(NULL, unload_pjsip, NULL);

	ast_threadpool_shutdown(sip_threadpool);

	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS | AST_MODFLAG_LOAD_ORDER, "Basic SIP resource",
		.load = load_module,
		.unload = unload_module,
		.reload = reload_module,
		.load_pri = AST_MODPRI_CHANNEL_DEPEND - 5,
);
