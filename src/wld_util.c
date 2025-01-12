/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/
#define _GNU_SOURCE
#include "wld_util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include <openssl/evp.h>

#include "swl/swl_assert.h"
#include "swla/swla_conversion.h"
#include "swla/swla_object.h"
#include "swl/swl_string.h"
#include "swl/swl_hex.h"
#include "swl/swl_common_chanspec.h"
#include "amxd/amxd_transaction.h"

#include "wld_linuxIfUtils.h"

#define ME "util"

const char* com_dir_char[COM_DIR_MAX] = {"Tx", "Rx"};

/*
** Country/Region Codes from MS WINNLS.H
** Numbering from ISO 3166 short names from Broadcom driver.
*/
const T_COUNTRYCODE Rad_CountryCode[] = {
    { 250, "European Union", "EU", SWL_OP_CLASS_COUNTRY_EU},
    { 250, "European Union 5G+", "EU2", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Afghanistan", "AF", SWL_OP_CLASS_COUNTRY_MAX},
    {   8, "Albania", "AL", SWL_OP_CLASS_COUNTRY_EU},
    {  12, "Algeria", "DZ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "American Samoa", "AS", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Angola", "AO", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Anguilla", "AI", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Antigua and Barbuda", "AG", SWL_OP_CLASS_COUNTRY_MAX },
    {  32, "Argentina", "AR", SWL_OP_CLASS_COUNTRY_MAX},
    {  51, "Armenia", "AM", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Aruba", "AM", SWL_OP_CLASS_COUNTRY_MAX},
    {  36, "Australia", "AU", SWL_OP_CLASS_COUNTRY_MAX},
    {  40, "Austria", "AT", SWL_OP_CLASS_COUNTRY_EU},
    {  31, "Azerbaijan", "AZ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Bahamas", "BS", SWL_OP_CLASS_COUNTRY_MAX },
    {  48, "Bahrain", "BH", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Baker Island", "OB", SWL_OP_CLASS_COUNTRY_MAX },
    {  50, "Bangladesh", "BD", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Barbados", "BB", SWL_OP_CLASS_COUNTRY_MAX},
    { 112, "Belarus", "BY", SWL_OP_CLASS_COUNTRY_EU},
    {  56, "Belgium", "BE", SWL_OP_CLASS_COUNTRY_EU},
    {  84, "Belize", "BZ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Benin", "BJ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Bermuda", "BM", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Bhutan", "BT", SWL_OP_CLASS_COUNTRY_MAX},
    {  68, "Bolivia", "BO", SWL_OP_CLASS_COUNTRY_MAX},
    {  70, "Bosnia and Herzegovina", "BA", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Botswana", "BW", SWL_OP_CLASS_COUNTRY_MAX},
    {  76, "Brazil", "BR", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "British Indian Ocean Territory", "IO", SWL_OP_CLASS_COUNTRY_MAX},
    {  96, "Brunei Darussalam", "BN", SWL_OP_CLASS_COUNTRY_MAX},
    { 100, "Bulgaria", "BG", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Burkina Faso", "BF", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Burundi", "BI", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Cambodia", "KH", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Cameroon", "CM", SWL_OP_CLASS_COUNTRY_MAX},
    { 124, "Canada", "CA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Cape Verde", "CV", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Cayman Islands", "KY", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Central African Republic", "CF", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Chad", "TD", SWL_OP_CLASS_COUNTRY_MAX},
    { 152, "Chile", "CL", SWL_OP_CLASS_COUNTRY_MAX},
    { 156, "China", "CN", SWL_OP_CLASS_COUNTRY_CN},
    {   0, "Christmas Island", "CX", SWL_OP_CLASS_COUNTRY_MAX},
    { 170, "Colombia", "CO", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Comoros", "KM", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Congo", "CG", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Congo, The Democratic Republic Of The", "CD", SWL_OP_CLASS_COUNTRY_MAX},
    { 188, "Costa Rica", "CR", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Cote D'ivoire", "CI", SWL_OP_CLASS_COUNTRY_MAX},
    { 191, "Croatia", "HR", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Cuba", "CU", SWL_OP_CLASS_COUNTRY_MAX},
    { 196, "Cyprus", "CY", SWL_OP_CLASS_COUNTRY_EU },
    { 203, "Czech Republic", "CZ", SWL_OP_CLASS_COUNTRY_EU},
    { 208, "Denmark", "DK", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Djibouti", "DJ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Dominica", "DM", SWL_OP_CLASS_COUNTRY_MAX},
    { 214, "Dominican Republic", "DO", SWL_OP_CLASS_COUNTRY_MAX},
    { 218, "Ecuador", "EC", SWL_OP_CLASS_COUNTRY_MAX},
    { 818, "Egypt", "EG", SWL_OP_CLASS_COUNTRY_MAX},
    { 222, "El Salvador", "SV", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Equatorial Guinea", "GQ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Eritrea", "ER", SWL_OP_CLASS_COUNTRY_MAX},
    { 233, "Estonia", "EE", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Ethiopia", "ET", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Falkland Islands (Malvinas)", "FK", SWL_OP_CLASS_COUNTRY_MAX},
    { 234, "Faroe Islands", "FO", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Fiji", "FJ", SWL_OP_CLASS_COUNTRY_EU},
    { 246, "Finland", "FI", SWL_OP_CLASS_COUNTRY_EU },
    { 250, "France", "FR", SWL_OP_CLASS_COUNTRY_EU},
    { 255, "French Guina", "GF", SWL_OP_CLASS_COUNTRY_MAX},
    { 255, "French Polynesia", "PF", SWL_OP_CLASS_COUNTRY_MAX },
    { 255, "French Southern Territories", "TF", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Gabon", "GA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Gambia", "GM", SWL_OP_CLASS_COUNTRY_MAX},
    { 268, "Georgia", "GE", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Ghana", "GH", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Gibraltar", "GI", SWL_OP_CLASS_COUNTRY_EU },
    { 276, "Germany", "DE", SWL_OP_CLASS_COUNTRY_EU},
    { 300, "Greece", "GR", SWL_OP_CLASS_COUNTRY_EU },
    {   0, "Grenada", "GD", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Guadeloupe", "GP", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Guam", "GU", SWL_OP_CLASS_COUNTRY_MAX},
    { 320, "Guatemala", "GT", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Guernsey", "GG", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Guinea", "GN", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Guinea-bissau", "GW", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Guyana", "GY", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Haiti", "HT", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Holy See (Vatican City State)", "VA", SWL_OP_CLASS_COUNTRY_EU},
    { 340, "Honduras", "HN", SWL_OP_CLASS_COUNTRY_MAX},
    { 344, "Hong Kong", "HK", SWL_OP_CLASS_COUNTRY_MAX},
    { 348, "Hungary", "HU", SWL_OP_CLASS_COUNTRY_EU },
    { 352, "Iceland", "IS", SWL_OP_CLASS_COUNTRY_EU},
    { 256, "India", "IN", SWL_OP_CLASS_COUNTRY_MAX},
    { 360, "Indonesia", "ID", SWL_OP_CLASS_COUNTRY_MAX},
    { 364, "Iran, Islamic Republic Of", "IR", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Iraq", "IQ", SWL_OP_CLASS_COUNTRY_MAX},
    { 372, "Ireland", "IE", SWL_OP_CLASS_COUNTRY_EU},
    { 376, "Israel", "IL", SWL_OP_CLASS_COUNTRY_MAX},
    { 380, "Italy", "IT", SWL_OP_CLASS_COUNTRY_EU},
    { 388, "Jamaica", "JM", SWL_OP_CLASS_COUNTRY_MAX},
    { 392, "Japan", "JP", SWL_OP_CLASS_COUNTRY_JP },
    { 393, "Japan_1", "J1", SWL_OP_CLASS_COUNTRY_JP },
    { 394, "Japan_2", "J2", SWL_OP_CLASS_COUNTRY_JP},
    { 395, "Japan_3", "J3", SWL_OP_CLASS_COUNTRY_JP},
    { 396, "Japan_4", "J4", SWL_OP_CLASS_COUNTRY_JP},
    { 397, "Japan_5", "J5", SWL_OP_CLASS_COUNTRY_JP},
    { 399, "Japan_6", "J6", SWL_OP_CLASS_COUNTRY_JP},
    {4007, "Japan_7", "J7", SWL_OP_CLASS_COUNTRY_JP},
    {4008, "Japan_8", "J8", SWL_OP_CLASS_COUNTRY_JP},
    {4009, "Japan_9", "J9", SWL_OP_CLASS_COUNTRY_JP},
    {4010, "Japan_10", "J10", SWL_OP_CLASS_COUNTRY_JP},
    {   0, "Jersey", "JE", SWL_OP_CLASS_COUNTRY_MAX},
    { 400, "Jordan", "JO", SWL_OP_CLASS_COUNTRY_JP},
    { 398, "Kazakhstan", "KZ", SWL_OP_CLASS_COUNTRY_MAX},
    { 404, "Kenya", "KE", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Kiribati", "KI", SWL_OP_CLASS_COUNTRY_MAX},
    { 410, "Korea, Republic Of", "KR", SWL_OP_CLASS_COUNTRY_MAX},
//408,      KOREA_NORTH             /* North Korea */
//410,      KOREA_ROC               /* South Korea */
//411,      KOREA_ROC2              /* South Korea */
    { 414, "Kuwait", "KW", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Kosovo (No country code, use 0A)", "0A", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Kyrgyzstan", "KG", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Lao People's Democratic Repubic", "LA", SWL_OP_CLASS_COUNTRY_MAX},
    { 428, "Latvia", "LV", SWL_OP_CLASS_COUNTRY_EU},
    { 422, "Lebanon", "LB", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Lesotho", "LS", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Liberia", "LR", SWL_OP_CLASS_COUNTRY_MAX},
    { 434, "Libyan Arab Jamahiriya", "LY", SWL_OP_CLASS_COUNTRY_MAX},
    { 438, "Liechtenstein", "LI", SWL_OP_CLASS_COUNTRY_EU },
    { 440, "Lithuania", "LT", SWL_OP_CLASS_COUNTRY_EU },
    { 442, "Luxembourg", "LU", SWL_OP_CLASS_COUNTRY_EU },
    { 446, "Macau", "MA", SWL_OP_CLASS_COUNTRY_MAX},
    { 807, "Macedonia", "MK", SWL_OP_CLASS_COUNTRY_EU },
    {   0, "Madagascar", "MG", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Malawi", "MW", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Macao", "MO", SWL_OP_CLASS_COUNTRY_MAX},
    { 458, "Malaysia", "MY", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Maldives", "MV", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Mali", "ML", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Malta", "MT", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Man, Isle Of", "IM", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Martinique", "MQ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Mauritania", "MR", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Mauritius", "MU", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Mayotte", "YT", SWL_OP_CLASS_COUNTRY_MAX},
    { 484, "Mexico", "MX", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Micronesia, Federated States Of", "FM", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Moldova, Republic Of", "MD", SWL_OP_CLASS_COUNTRY_EU},
    { 492, "Monaco", "MC", SWL_OP_CLASS_COUNTRY_EU },
    {   0, "Mongolia", "MN", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Montenegro", "ME", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Montserrat", "MS", SWL_OP_CLASS_COUNTRY_MAX},
    { 504, "Morocco", "MA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Mozambique", "MZ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Myanmar", "MM", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Namibia", "NA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Nauru", "NR", SWL_OP_CLASS_COUNTRY_MAX },
    { 524, "Nepal", "NP", SWL_OP_CLASS_COUNTRY_MAX},
    { 528, "Netherlands", "NL", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Netherlands Antilles", "AN", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "New Caledonia", "NC", SWL_OP_CLASS_COUNTRY_MAX },
    { 554, "New Zealand", "NZ", SWL_OP_CLASS_COUNTRY_MAX},
    { 558, "Nicaragua", "NI", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Niger", "NE", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Nigeria", "NG", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Niue", "NU", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Norfolk Island", "NF", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Northern Mariana Islands", "MP", SWL_OP_CLASS_COUNTRY_MAX},
    { 578, "Norway", "NO", SWL_OP_CLASS_COUNTRY_EU},
    { 512, "Oman", "OM", SWL_OP_CLASS_COUNTRY_MAX},
    { 586, "Pakistan", "PK", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Palau", "PW", SWL_OP_CLASS_COUNTRY_MAX},
    { 591, "Panama", "PA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Papua New Guinea", "PG", SWL_OP_CLASS_COUNTRY_MAX},
    { 600, "Paraguay", "PY", SWL_OP_CLASS_COUNTRY_MAX},
    { 604, "Peru", "PE", SWL_OP_CLASS_COUNTRY_MAX},
    { 608, "Philippines", "PH", SWL_OP_CLASS_COUNTRY_MAX },
    { 616, "Poland", "PL", SWL_OP_CLASS_COUNTRY_EU},
    { 620, "Portugal", "PT", SWL_OP_CLASS_COUNTRY_EU},
    { 630, "Pueto Rico", "PR", SWL_OP_CLASS_COUNTRY_MAX},
    { 634, "Qatar", "QA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Reunion", "RE", SWL_OP_CLASS_COUNTRY_MAX},
    { 642, "Romania", "RO", SWL_OP_CLASS_COUNTRY_EU },
    {   0, "Rwanda", "RW", SWL_OP_CLASS_COUNTRY_MAX},
    { 643, "Russia", "RU", SWL_OP_CLASS_COUNTRY_EU },
    {   0, "Saint Kitts and Nevis", "KN", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Saint Lucia", "LC", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Sanit Martin / Sint Marteen", "MF", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Saint Pierre and Miquelon", "PM", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Saint Vincent and The Grenadines", "VC", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Samoa", "WS", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "San Marino", "SM", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Sao Tome and Principe", "ST", SWL_OP_CLASS_COUNTRY_MAX },
    { 682, "Saudi Arabia", "SA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Senegal", "SN", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Serbia", "RS", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Seychelles", "SC", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Sierra Leone", "SL", SWL_OP_CLASS_COUNTRY_MAX},
    { 702, "Singapore", "SG", SWL_OP_CLASS_COUNTRY_MAX},
    { 703, "Slovakia", "SK", SWL_OP_CLASS_COUNTRY_EU},
    { 705, "Slovenia", "SI", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Solomon Islands", "SB", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Somalia", "SO", SWL_OP_CLASS_COUNTRY_MAX},
    { 710, "South Africa", "ZA", SWL_OP_CLASS_COUNTRY_MAX},
    { 724, "Spain", "ES", SWL_OP_CLASS_COUNTRY_EU},
    { 144, "Sri Lanka", "LK", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Suriname", "SR", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Swaziland", "SZ", SWL_OP_CLASS_COUNTRY_MAX},
    { 752, "Sweden", "SE", SWL_OP_CLASS_COUNTRY_EU},
    { 756, "Switzerland", "CH", SWL_OP_CLASS_COUNTRY_EU},
    { 760, "Syria", "SY", SWL_OP_CLASS_COUNTRY_MAX},
    { 158, "Taiwan, Province Of China", "TW", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Tajikistan", "TJ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Tanzania, United Republic Of", "TZ", SWL_OP_CLASS_COUNTRY_MAX},
    { 764, "Thailand", "TH", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Togo", "TG", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Tonga", "TO", SWL_OP_CLASS_COUNTRY_MAX},
    { 780, "Trinidad and Tobago", "TT", SWL_OP_CLASS_COUNTRY_MAX},
    { 788, "Tunisia", "TN", SWL_OP_CLASS_COUNTRY_MAX},
    { 792, "Turkey", "TR", SWL_OP_CLASS_COUNTRY_EU},
    {   0, "Turkmenistan", "TM", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Turks and Caicos Islands", "TC", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Tuvalu", "TV", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Uganda", "UG", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Ukraine", "UA", SWL_OP_CLASS_COUNTRY_EU},
    { 784, "United Arab Emirates", "AE", SWL_OP_CLASS_COUNTRY_MAX},
    { 826, "United Kingdom", "GB", SWL_OP_CLASS_COUNTRY_EU},
    { 840, "United States", "US", SWL_OP_CLASS_COUNTRY_USA },
    { 842, "United States (No DFS)", "Q2", SWL_OP_CLASS_COUNTRY_USA },
    {   0, "United States Minor Outlying Islands", "UM", SWL_OP_CLASS_COUNTRY_USA},
    { 858, "Uruguay", "UY", SWL_OP_CLASS_COUNTRY_MAX },
    { 860, "Uzbekistan", "UZ", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Vanuatu", "VU", SWL_OP_CLASS_COUNTRY_MAX },
    { 862, "Venezuela", "VE", SWL_OP_CLASS_COUNTRY_MAX},
    { 704, "Viet Nam", "VN", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Virgin Islands, British", "VG", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "Virgin Islands, U.S.", "VI", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Wallis and Futuna", "WF", SWL_OP_CLASS_COUNTRY_MAX },
    {   0, "West Bank", "0C", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Western Sahara", "EH", SWL_OP_CLASS_COUNTRY_MAX},
    { 887, "Yemen", "YE", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Zambia", "ZM", SWL_OP_CLASS_COUNTRY_MAX},
    { 716, "Zimbabwe", "ZW", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Country Z2", "Z2", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Europe / APAC 2005", "XA", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "North and South America and Taiwan", "XB", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "FCC Worldwide", "X0", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide APAC", "X1", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide RoW 2", "X2", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "ETSI", "X3", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide Safe Enhanced Mode Locale", "XS", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide Locale for Linux driver", "XW", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide Locale", "XX", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Fake Country Code", "XY", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide Locale", "XZ", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "European Locale 0dBi", "XU", SWL_OP_CLASS_COUNTRY_MAX},
    {   0, "Worldwide Safe Mode Locale", "XV", SWL_OP_CLASS_COUNTRY_MAX},

    {   0, NULL, NULL, SWL_OP_CLASS_COUNTRY_MAX}
};

bool __WLD_BUG_ON(bool bug, const char* condition _UNUSED, const char* function _UNUSED,
                  const char* file _UNUSED, int line _UNUSED) {
    if(!bug) {
        return false;
    }

    SAH_TRACEZ_ERROR(ME, "WLD_BUG_ON(\"%s\") in function %s() (%s:%d)\n",
                     condition, function, file, line);
    return true;
}


int debugIsVapPointer(void* p) {
    T_AccessPoint* pAP = (T_AccessPoint*) p;
    if(pAP && (pAP->debug == VAP_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID VAP pointer %p", p);
    return 0;
}

int debugIsEpPointer(void* p) {
    T_EndPoint* pEP = (T_EndPoint*) p;
    if(pEP && (pEP->debug == ENDP_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID EndPoint pointer %p", p);
    return 0;
}

int debugIsRadPointer(void* p) {
    T_Radio* pR = (T_Radio*) p;
    if(pR && (pR->debug == RAD_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID RADIO pointer %p", p);
    return 0;
}

int debugIsSsidPointer(void* p) {
    T_SSID* pS = (T_SSID*) p;

    if(pS && (pS->debug == SSID_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID SSID pointer %p", p);
    return 0;
}

/**
 * Function returns true when the string (str) can be found in
 * the string array list (strarray).
 *
 * @param str NULL terminated text string
 * @param strarray List of strings, the list must end with a
 *                 NULL pointer..
 * @param index If given we return here the index value of the
 *              hitted string.
 *
 * @return bool true when str is found in strarray else false.
 */
bool findStrInArray(const char* str, const char** strarray, int* index) {
    int i;

    for(i = 0; strarray && strarray[i]; i++) {
        if(!strcmp(str, strarray[i])) {
            if(index) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

int findStrInArrayN(const char* str, const char** strarray, int size, int default_index) {
    int i = 0;
    for(i = 0; i < size && str && strarray && strarray[i]; i++) {
        if(!strcmp(str, strarray[i])) {
            return i;
        }
    }
    return default_index;
}

/**
 * Convert a list of integers into a comma separated list.
 * The comma separated list will go into str.
 */
bool convIntArrToString(char* str, int str_size, const int* int_list, int list_size) {
    int i = 0;
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_NOT_NULL(int_list, false, ME, "NULL");
    ASSERT_TRUE(str_size > 0, false, ME, "null str size");
    str[0] = 0;
    ASSERTS_TRUE(list_size > 0, true, ME, "null list size");
    for(i = 0; (i < list_size) && ((int) strlen(str) < (str_size - 1)); i++) {
        swl_strlst_catFormat(str, str_size, ",", "%i", int_list[i]);
    }
    ASSERTI_EQUALS(i, list_size, false, ME, "Int array not fully converted to string: str buf too small");
    return true;
}

bool convStrToIntArray(int* int_list, size_t list_size, const char* str, int* outSize) {
    if(outSize != NULL) {
        *outSize = 0;
    }
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_NOT_NULL(int_list, false, ME, "NULL");
    ASSERT_TRUE(list_size > 0, false, ME, "INVALID");

    size_t i = 0;
    uint32_t size = strlen(str) + 1;
    char buffer[size];
    swl_str_copy(buffer, sizeof(buffer), str);
    buffer[size - 1] = '\0';
    char* needle = buffer;

    do {
        char* sep = strchr(needle, ',');
        if(sep != NULL) {
            *sep = '\0';
        }
        int_list[i] = atoi(needle);
        i++;
        needle = (sep != NULL) ? sep + 1 : NULL;
    } while(needle != NULL && i < list_size);
    if(outSize != NULL) {
        *outSize = i;
    }
    return needle == NULL;
}

bool convStrArrToStr(char* str, int str_size, const char** strList, int list_size) {
    int i = 0;
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_NOT_NULL(strList, false, ME, "NULL");
    ASSERT_TRUE(str_size > 0, false, ME, "null str size");
    str[0] = 0;
    ASSERTS_TRUE(list_size > 0, true, ME, "null list size");
    for(i = 0; (i < list_size) && ((int) strlen(str) < (str_size - 1)); i++) {
        swl_strlst_cat(str, str_size, ",", strList[i]);
    }
    ASSERTI_EQUALS(i, list_size, false, ME, "Int array not fully converted to string: str buf too small");
    return true;
}

/**
 * Internal function to free all elements in a linked listed. Preferably to be called with the
 * macro llist_freeData
 * Offset needs to be the offset of iterator in the list object structure
 */
void wldu_llist_freeDataInternal(amxc_llist_t* list, size_t offset) {
    while(!amxc_llist_is_empty(list)) {
        amxc_llist_it_t* it = amxc_llist_take_first(list);
        void* object = (((void*) it) - offset);
        free(object);
    }
}

/**
 * Internal function to destroy all elements in a linked listed, i.e. free them by calling the destroy function
 * Preferably to be called with the macro llist_freeData.
 * Offset needs to be the offset of iterator in the list object structure
 */
void wldu_llist_freeDataFunInternal(amxc_llist_t* list, size_t offset, void (* destroy)(void* val)) {
    ASSERT_NOT_NULL(destroy, , ME, "NULL");
    amxc_llist_it_t* it;
    while(!amxc_llist_is_empty(list)) {
        it = amxc_llist_take_first(list);
        void* object = (((void*) it) - offset);
        destroy(object);
    }
}


/**
 * Internal function to map a function all elements in a linked listed.
 * Preferably to be called with the macro llist_map.
 * Offset needs to be the offset of iterator in the list object structure
 */
void wldu_llist_mapInternal(amxc_llist_t* list, size_t offset, void (* map)(void* val, void* data), void* data) {
    ASSERT_NOT_NULL(map, , ME, "NULL");
    amxc_llist_for_each(it, list) {
        void* object = (((void*) it) - offset);
        map(object, data);
    }
}

unsigned long conv_ModeBitStr(char** enumStrList, char* str) {
    unsigned long val, i;
    char* pCh = NULL;

    for(i = 0, val = 0; enumStrList[i]; i++) {
        pCh = strstr(str, enumStrList[i]);
        if(pCh) {
            val |= (1UL << i);
        }
    }
    return val;
}

long conv_ModeIndexStr(const char** enumStrList, const char* str) {
    ASSERT_NOT_NULL(str, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(enumStrList, WLD_ERROR, ME, "NULL");

    long i = 0;

    for(i = 0; enumStrList[i]; i++) {
        if(!strcmp(str, enumStrList[i])) {
            return i;
        }
    }

    return WLD_ERROR;
}

bool conv_maskToStrSep(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize, char separator) {
    ASSERT_NOT_NULL(enumStrList, false, ME, "NULL");
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_FALSE(strSize == 0, false, ME, "NULL");
    ASSERT_NOT_EQUALS(separator, '\0', false, ME, "Zero separator");
    uint32_t i = 0;
    uint32_t index = 0;
    int printed = 0;
    str[0] = '\0';

    for(i = 0; i < maxVal; i++) {
        if(mask & (1 << i)) {
            if(index) {
                printed = snprintf(&str[index], strSize - index, "%c%s", separator, enumStrList[i]);
            } else {
                printed = snprintf(&str[index], strSize - index, "%s", enumStrList[i]);
            }
            index += printed;

            ASSERT_TRUE(index < strSize, false, ME, "Buff too small");
        }
    }
    return true;
}

bool conv_maskToStr(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize) {
    return conv_maskToStrSep(mask, enumStrList, maxVal, str, strSize, ',');
}

uint32_t conv_strToEnum(const char** enumStrList, const char* str, uint32_t maxVal, uint32_t defaultVal) {
    return swl_conv_charToEnum(str, enumStrList, maxVal, defaultVal);
}


/**
 * @name conv_ModeIndexStrEx
 *
 * @details Function looks for a given string in a string array buffer.
 * The number of items is limited by the string array or the min-maxIndex range.
 *
 *
 * @param str NULL terminated text string
 * @param strarray List of strings, the list must end with a NULL pointer.
 * @param minIndex Starting index number
 * @param maxIndex Ending index number
 *
 * @return int index value that contains the matching string in the strarray ELSE -1!
 */
int conv_ModeIndexStrEx(const char** enumStrList, const char* str, int minIdx, int maxIdx) {
    int i, ret = -1;

    if(str && enumStrList) {
        for(i = minIdx; i <= maxIdx && enumStrList[i]; i++) {
            if(strcmp(enumStrList[i], str)) {
                continue;
            }
            ret = i;
        }
    }
    return ret;
}

/** Try to reduce the use of this function, % and / are
 *  slowing down the process a bit. */
char* itoa(int value, char* result, int base) {
    char aux;
    char* begin, * end;
    char* out;
    unsigned int quotient;

    if((base < 2) || (base > 16)) {
        *result = 0; return result;
    }

    out = result;
    begin = out;

    quotient = ( value < 0 && base == 10) ? (unsigned int) (-value) : (unsigned int) value;

    do {
        *out = "0123456789ABCDEF"[quotient % base];
        ++out;
        quotient /= base;
    } while(quotient);

    // Only apply negative sign for base 10
    if((value < 0) && (base == 10)) {
        *out++ = '-';
    }

    end = out;
    *out = 0;

    /* reverse string */
    end--;
    while(end > begin) {
        aux = *end, *end-- = *begin, *begin++ = aux;
    }

    return result;
}

/**
 * @brief find the lowest bit in an array of longs. (used
 *       in our FSM)
 *
 * @param pval  pointer to an unsigned long array
 * @param elem The amount of longs stored int the array
 *
 * @return int The lowest bit that's set. In case no bit is set
 *         the return is < 0. (-1)
 */
int getLowestBitLongArray(unsigned long* pval, int elem) {
    int ret = 0, i;
    unsigned long sv;

    for(i = 0; pval && i < elem; i++) {
        sv = pval[i];
        if(sv) {
            ret += (i << 5);
            /* Filter out lowest active bit */
            sv &= -sv;
            /* Calculate the bit index */
            if((sv & 0x0000ffff) == 0) {
                ret += 16;
            }
            if((sv & 0x00ff00ff) == 0) {
                ret += 8;
            }
            if((sv & 0x0f0f0f0f) == 0) {
                ret += 4;
            }
            if((sv & 0x33333333) == 0) {
                ret += 2;
            }
            if((sv & 0x55555555) == 0) {
                ret += 1;
            }
            return ret;
        }
    }
    return -1;
}

/**
 * @brief find the highest bit in an array of longs.
 *
 * @param pval  pointer to an unsigned long array
 * @param elem The amount of longs stored int the array
 *
 * @return int The highest bit that's set. In case no bit is set
 *         the return is < 0. (-1)
 */
int getHighestBitLongArray(unsigned long* pval, int elem) {
    int ret, i;
    unsigned long val, sv;

    for(i = (elem - 1); pval && i >= 0; i--) {
        sv = pval[i];
        if(sv) {
            ret = (i << 5);
            val = sv >> 16; if(val) {
                sv = val; ret += 16;
            }
            val = sv >> 8;  if(val) {
                sv = val; ret += 8;
            }
            val = sv >> 4;  if(val) {
                sv = val; ret += 4;
            }
            val = sv >> 2;  if(val) {
                sv = val; ret += 2;
            }
            val = sv >> 1;  if(val) {
                ret += 1;
            }
            return ret;
        }
    }
    return -1;
}

int areBitsSetLongArray(unsigned long* pval, int elem) {
    int i;
    unsigned long sv;

    for(i = 0, sv = 0; i < elem && pval; i++) {
        sv |= pval[i];
    }
    return (sv) ? 1 : 0;
}

int clearAllBitsLongArray(unsigned long* pval, int elem) {
    int i;

    for(i = 0; i < elem && pval; i++) {
        pval[i] = 0;
    }
    return 0;
}

/**
 * Returns 1 if the given bit number, and only the given bitnr is set in the long array.
 * Returns 0 otherwise.
 */
int isBitOnlySetLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long test = 0;
    for(i = 0; i < elem && pval; i++) {

        if((bitNr < 32) && (bitNr >= 0)) {
            test = 1 << bitNr;
        } else {
            test = 0;
        }

        if(pval[i] != test) {
            return 0;
        }
        bitNr -= 32;
    }
    return 1;
}

int areBitsOnlySetLongArray(unsigned long* pval, int elem, int* bitNrArray, int nrBitElem) {
    int i = 0;
    int j = 0;
    unsigned long mask;

    for(i = 0; i < elem && pval; i++) {
        mask = 0xffffffff;

        for(j = 0; j < nrBitElem; j++) {
            int bitNr = bitNrArray[j] - (i * 32);
            if((bitNr & ~0x1f) == 0) {
                mask &= ~(1UL << bitNr);
            }
        }

        if((pval[i] & mask) > 0) {
            return 0;
        }
    }
    return 1;
}

int isBitSetLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;

    while(bitNr > 0x1f) {
        bitNr -= 0x20;
        i++;
    }
    if((i >= elem) || !pval) {
        return 0;
    }
    sv = 1 << (bitNr & 0x1f);
    return (pval[i] & sv) ? 1 : 0;
}

int setBitLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;
    while(bitNr > 0x1f) {
        bitNr -= 0x20;
        i++;
    }
    if((i >= elem) || !pval) {
        return 0;
    }
    sv = 1 << (bitNr & 0x1f);
    pval[i] |= sv;

    return 1;
}

int clearBitLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;
    while(bitNr > 0x1f) {
        bitNr -= 0x20;
        i++;
    }

    if((bitNr < 0) || (i >= elem) || !pval) {
        return 0;
    }

    sv = 1 << (bitNr & 0x1f);
    pval[i] &= ~sv;

    return 1;
}

int markAllBitsLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;
    while(bitNr > 0x1f) {
        pval[i] = 0xffffffff;
        i++;
        bitNr -= 0x20;
    }
    if((bitNr < 0) || (i >= elem) || !pval) {
        return 0;
    }

    sv = (1 << bitNr) - 1;
    pval[i] = sv;

    return 1;
}

/*
   Active Commit FSM Bits,
   Shield ongoing vendor commit activities from new/inbetween configuration changes.
 */
void moveFSM_VAPBit2ACFSM(T_AccessPoint* pAP) {
    int i;

    for(i = 0; pAP && i < FSM_BW && pAP->fsm.FSM_BitActionArray[i]; i++) {
        pAP->fsm.FSM_AC_BitActionArray[i] = pAP->fsm.FSM_BitActionArray[i];
        pAP->fsm.FSM_BitActionArray[i] = 0;
    }
}

void moveFSM_RADBit2ACFSM(T_Radio* pR) {
    int i;

    for(i = 0; pR && i < FSM_BW && pR->fsmRad.FSM_BitActionArray[i]; i++) {
        pR->fsmRad.FSM_AC_BitActionArray[i] = pR->fsmRad.FSM_BitActionArray[i];
        pR->fsmRad.FSM_BitActionArray[i] = 0;
    }
}

void moveFSM2ACFSM(T_FSM* fsm) {
    int i;

    for(i = 0; fsm && i < FSM_BW && fsm->FSM_BitActionArray[i]; i++) {
        fsm->FSM_AC_BitActionArray[i] = fsm->FSM_BitActionArray[i];
        fsm->FSM_BitActionArray[i] = 0;
    }
}

void longArrayCopy(unsigned long* to, unsigned long* from, int len) {
    int i;
    if(!to || !from) {
        return;
    }

    for(i = 0; i < len; i++) {
        to[i] = from[i];
    }
}

void longArrayBitOr(unsigned long* to, unsigned long* from, int len) {
    int i;
    if(!to || !from) {
        return;
    }

    for(i = 0; i < len; i++) {
        to[i] |= from[i];
    }
}

void longArrayClean(unsigned long* array, int len) {
    int i;
    if(!array) {
        return;
    }

    for(i = 0; i < len; i++) {
        array[i] = 0;
    }
}

/**
 * @details isValidWEPKey check if given string is a valid WEP
 *          key. We support 5/13/16 ASCII WEP key or 10/26/32
 *          Hex WEP key
 *
 * @param key NULL terminated WEP key string.
 *
 * @return due to legacy reasons, this returns the WEP standard for which this
 * key is suited. Possible return values are
 * * SWL_SECURITY_APMODE_WEP64
 * * SWL_SECURITY_APMODE_WEP128
 * * SWL_SECURITY_APMODE_WEP128IV
 */
swl_security_apMode_e isValidWEPKey (const char* key) {
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    int len = strlen(key);
    int i;

    if((len == 10) || (len == 26) || (len == 32)) {
        /* Expect a HEX string */
        for(i = 0; i < len; i++) {
            if(!(isxDigit(key[i]))) {
                return 0;
            }
        }
        return (len == 10) ? (SWL_SECURITY_APMODE_WEP64) : ((len == 26) ? SWL_SECURITY_APMODE_WEP128 : SWL_SECURITY_APMODE_WEP128IV);
    } else {
        /* Expect an ASCII string of fix lenght */
        if((len == 5) || (len == 13) || (len == 16)) {
            return (len == 5) ? (SWL_SECURITY_APMODE_WEP64) : ((len == 13) ? SWL_SECURITY_APMODE_WEP128 : SWL_SECURITY_APMODE_WEP128IV);
        }
    }
    /* All other stuff... */
    return SWL_SECURITY_APMODE_UNKNOWN;
}

/**
 * Function converts the binary WEP key to an readable ASCII
 * string.
 */
char* convASCII_WEPKey(const char* key, char* out, int sizeout) {
    int len, i;
    unsigned char ch;

    if(key) {
        len = strlen(key);
        if(out &&
           (( len == 5) || ( len == 13) || ( len == 16)) &&
           (sizeout > (len * 2))) {
            for(i = 0; i < len; i++) {
                ch = key[i];
                out[(i << 1) + 0] = "0123456789ABCDEF"[((ch & 0xf0) >> 4)];
                out[(i << 1) + 1] = "0123456789ABCDEF"[((ch & 0x0f) >> 0)];
            }
            out[(i << 1)] = '\0';
            return out;
        }
        // All other 10/26/32 can be used directly!
        if(((len == 10) || (len == 26) || (len == 32))) {
            return (char*) key;
        }
    }
    return NULL;
}

/**
 * Convets the readable ASCII string to a binary WEP key
 */
char* convHex_WEPKey(const char* key, char* out, int sizeout) {
    int len, i;
    unsigned char* ch;

    if(key) {
        len = strlen(key);
        if((len == 5) || (len == 13) || (len == 16)) {
            return (char*) key; // Current given key is already valid.
        }

        if(out &&
           (((len == 10) && (sizeout >= 5)) ||
            ((len == 26) && (sizeout >= 13)) ||
            ((len == 32) && (sizeout >= 16)))) {
            ch = (unsigned char*) key;
            for(i = 0; i < len; i++, ch++) {
                if(i & 1) {
                    out[i >> 1] <<= 4;
                } else {
                    out[i >> 1] = 0;
                }

                if((*ch >= '0') && (*ch <= '9')) {
                    out[i >> 1] += *ch - '0';
                } else if(( *ch >= 'a') && ( *ch <= 'f')) {
                    out[i >> 1] += 10 + *ch - 'a';
                } else if(( *ch >= 'A') && ( *ch <= 'F')) {
                    out[i >> 1] += 10 + *ch - 'A';
                }
            }
            return out;
        }
    }
    return NULL;
}

/**
 * Check for a correct PSK key.
 */
bool isValidPSKKey(const char* key) {
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    int len = strlen(key);
    int i;
    char* CapKey = NULL;

    if(len == 64) {           // Checked, must be 64 bytes!
        CapKey = (char*) key; // fix error: assignment of read-only location
        for(i = 0; i < len; i++) {
            if(!(isxDigit(key[i]))) {
                return 0;
            }
            CapKey[i] = toupper(key[i]);    // Convert direct to upper chars
        }
        return true;
    }
    return false;
}

/**
 * Check for \n and \r in KeyPassPhrase.
 */
bool isSanitized (const char* pname) {
    ASSERTS_NOT_NULL(pname, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Checking KeyPassPhrase validity %s", pname);
    int i = 0;
    while(pname[i]) {
        if(pname[i] < 32) {
            return false;
        }
        i++;
    }
    return true;
}

/**
 * Check for a correct AES key.
 */
bool isValidAESKey(const char* key, int maxLen) {
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    bool sanitized = isSanitized(key);
    int len = strlen(key);
    if(!sanitized) {
        return false;
    }
    if(len < 8) {
        return false;
    }
    if(len > maxLen) {
        return false;
    }
    // Ignore null character
    if(len == maxLen) {
        int i = 0;
        for(i = 0; i < len; i++) {
            if(!isxDigit(key[i])) {
                return false;
            }
        }
    }
    return true;
}


/**
 * Check for a correct SSID name.
 */
bool isValidSSID(const char* ssid) {
    // ssid may not be empty!
    ASSERTI_STR(ssid, false, ME, "empty");

    // Smaller then 33 chars?
    int i = strlen(ssid);
    return (i <= 32);
}

/**
 * @name isListOfChannels_US
 *
 * @details This function returns TRUE when US channels are covered for the selected band.
 *
 * @param possibleChannels, array of channels must terminated by a 0 channel
 * @param len, number of elements in possibleChannels
 * @param band, which band must be checked for US mode?
 *
 * @return int
 */
int isListOfChannels_US(unsigned char* possibleChannels, int len, int band) {
    int i, US, EU;

    US = 0;
    EU = 0;

    if(!(possibleChannels && len)) {
        return -1;
    }

    switch(band) {
    case SWL_FREQ_BAND_2_4GHZ:
        for(i = 0; i < len && possibleChannels[i]; i++) {
            US += (possibleChannels[i] <= 11) ? 1 : 0;
            EU += (possibleChannels[i] > 11 && possibleChannels[i] <= 14) ? 1 : 0;
        }
        // For US must contain a valid 2.4 Channel but not one from EU!
        return (US && !EU);

    case SWL_FREQ_BAND_5GHZ:
        for(i = 0; i < len && possibleChannels[i]; i++) {
            EU += ( possibleChannels[i] <= 36) ? 1 : 0;
            US += ( possibleChannels[i] >= 144) ? 1 : 0;
        }
        // For US has more 5GHz channel then EU!
        return (US && EU);
    case SWL_FREQ_BAND_6GHZ:
        break;
    default:
        break;
    }
    return -1; // We don't know
}

/* The following function have been borrowed and adapted from hostapd*/
int get_random(unsigned char* buf, size_t len) {
    FILE* f;
    size_t rc;

    f = fopen("/dev/urandom", "rb");
    if(f == NULL) {
        SAH_TRACEZ_ERROR(ME, "Cannot open /dev/urandom.");
        return -1;
    }

    rc = fread(buf, 1, len, f);
    fclose(f);

    return rc != len ? -1 : 0;
}

/**
 * @name get_randomstr
 *
 * @details Generates a random NULL terminated string in the buffer!
 *
 * @param buf Destination pointer of string, must at least cover len+1 size! (NULL char)
 * @param len Length of generated key!
 *
 * @return int returns length of string.
 */
int get_randomstr(unsigned char* buf, size_t len) {
    const char RND_ASCII[64] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
    unsigned int i = 0;

    get_random(buf, len);
    for(i = 0; i < len; i++) {
        buf[i] = RND_ASCII[ (buf[i] & 0x3f) ];
    }
    buf[len] = '\0';

    return i;
}

/**
 * @name get_randomhexstr
 *
 * @details Generates a random hexadecimal NULL terminated string in the buffer!
 *
 * @param buf Destination pointer of string, must at least cover len+1 size! (NULL char)
 * @param len Length of generated key!
 *
 * @return int returns length of string.
 */
int get_randomhexstr(unsigned char* buf, size_t len) {
    const char RND_HEX[16] = "0123456789ABCDEF";
    unsigned int i = 0;

    get_random(buf, len);
    for(i = 0; i < len; i++) {
        buf[i] = RND_HEX[ (buf[i] & 0xf) ];
    }
    buf[len] = '\0';

    return i;
}

unsigned long wpsInitPin() {
    unsigned long pin = 0;

    if(0 != get_random((unsigned char*) &pin, sizeof(unsigned long))) {
        SAH_TRACEZ_ERROR(ME, "Can't get a random number from urandom, using rand and srand instead");
        srand(time(0));     /* get random numbers */
        pin = rand();
    }
#if WPS_PIN_LEN == 8
    pin %= 100000000;
#else
    pin %= 10000;
#endif
    return pin;
}

#if WPS_PIN_LEN == 8 //WPS8PIN
/* pin_gen -- generate an 8 character PIN number.
 */
void wpsPinGen(char pwd[WPS_PIN_LEN + 1]) {
    unsigned long pin;
    unsigned char checksum;
    unsigned long acc = 0;
    unsigned long tmp;

    SAH_TRACEZ_IN(ME);

    pin = wpsInitPin();
    SAH_TRACEZ_INFO(ME, "pin from wpsInitPin: %lu", pin);

    /* use random numbers between [10000000, 99999990)
     * so that first digit is never a zero, which could be confusing.
     */
    pin = 1000000 + pin % 9000000;
    tmp = pin * 10;

    acc += 3 * ((tmp / 10000000) % 10);
    acc += 1 * ((tmp / 1000000) % 10);
    acc += 3 * ((tmp / 100000) % 10);
    acc += 1 * ((tmp / 10000) % 10);
    acc += 3 * ((tmp / 1000) % 10);
    acc += 1 * ((tmp / 100) % 10);
    acc += 3 * ((tmp / 10) % 10);

    checksum = (unsigned char) (10 - (acc % 10)) % 10;
    SAH_TRACEZ_INFO(ME, "checksum=: %d", checksum);
    snprintf(pwd, WPS_PIN_LEN + 1, "%lu", pin * 10 + checksum);
    SAH_TRACEZ_INFO(ME, "pin after checksum: %lu, %s", pin * 10 + checksum, pwd);
    SAH_TRACEZ_OUT(ME);
}
#else
/* WPS_PIN_LEN == 4 */
void wpsPinGen(char pwd[WPS_PIN_LEN + 1]) {
    unsigned long pin;
    unsigned char checksum;
    unsigned long acc = 0;
    unsigned long tmp;

    SAH_TRACEZ_IN(ME);

    pin = wpsInitPin();
    SAH_TRACEZ_INFO(ME, "pin from wpsInitPin: %lu", pin);

    /* use random numbers between [1000, 9990)
     * so that first digit is never a zero, which could be confusing.
     */
    pin = 100 + pin % 900;
    tmp = pin * 10;

    acc += 3 * ((tmp / 1000) % 10);
    acc += 1 * ((tmp / 100) % 10);
    acc += 3 * ((tmp / 10) % 10);

    checksum = (unsigned char) (10 - (acc % 10)) % 10;
    snprintf(pwd, WPS_PIN_LEN + 1, "%lu", pin * 10 + checksum);
    SAH_TRACEZ_INFO(ME, "pin after checksum: %lu, %s", pin * 10 + checksum, pwd);
    SAH_TRACEZ_OUT(ME);
}
#endif

int wpsPinValid(unsigned long PIN) {
    unsigned long int accum = 0;

    // 4-Digit PIN [0000..9999]
    if(PIN <= 9999) {
        return TRUE;
    }
    // 8-Digit PIN
    accum += 3 * ((PIN / 10000000) % 10);
    accum += 1 * ((PIN / 1000000) % 10);
    accum += 3 * ((PIN / 100000) % 10);
    accum += 1 * ((PIN / 10000) % 10);
    accum += 3 * ((PIN / 1000) % 10);
    accum += 1 * ((PIN / 100) % 10);
    accum += 3 * ((PIN / 10) % 10);
    accum += 1 * ((PIN / 1) % 10);

    return (0 == (accum % 10));
}

/*
 * @brief checks if provided pin string has a valid numerical value
 * Cf. usp tr181 v2.15.0:
 * Device PIN used for PIN based pairing between WPS peers.
 * This PIN is either a four digit number or an eight digit number.
 * @param pinStr PIN string
 * @return true when PIN string matches validity criteria (i.e number with 4 or 8 digits)
 *         false otherwise
 */
bool wldu_checkWpsPinStr(const char* pinStr) {
    ASSERT_STR(pinStr, false, ME, "pin empty");
    uint32_t pinNum = 0;
    swl_rc_ne ret = wldu_convStrToNum(pinStr, &pinNum, sizeof(pinNum), 0, false);
    ASSERTI_EQUALS(ret, SWL_RC_OK, false, ME, "%s is not a number", pinStr);
    if((swl_str_len(pinStr) != 4) && (swl_str_len(pinStr) != 8)) {
        SAH_TRACEZ_ERROR(ME, "%s has not required pin length (4 or 8 digits)", pinStr);
        return false;
    }
    ASSERT_TRUE(wpsPinValid(pinNum), false, ME, "%s has invalid pin value", pinStr);
    return true;
}

/**
 * Get the uint8 list from a  comma separated string list.
 * @arg srcStrList:  comma separated list of channels
 * returns number of elements obtained from the list
 */
int32_t wldu_convStrToUInt8Arr(uint8_t* tgtList, uint tgtListSize, char* srcStrList) {
    ASSERT_NOT_NULL(srcStrList, -1, ME, "NULL");
    ASSERT_NOT_NULL(tgtList, -1, ME, "NULL");
    ASSERT_TRUE(tgtListSize > 0, -1, ME, "null ChanListSize");
    char tmp[32] = {0};
    unsigned int i = 0;
    int len = 0;

    do {
        if(i >= tgtListSize) {
            SAH_TRACEZ_ERROR(ME, "Source Str List is too large %d/%d", i, tgtListSize);
            return -1;
        }
        len = strcspn(srcStrList, ",");
        snprintf(tmp, sizeof(tmp), "%.*s", (int) len, srcStrList);
        tgtList[i] = atoi(tmp);
        i++;
        srcStrList += len;
    } while(*srcStrList++);

    return i;
}

int32_t wldu_convStr2Mac(unsigned char* mac, uint32_t sizeofmac, const char* str, uint32_t sizeofstr) {
    ASSERT_NOT_NULL(mac, 0, ME, "NULL");
    ASSERT_NOT_NULL(str, 0, ME, "NULL");
    ASSERT_TRUE(sizeofstr > 0, 0, ME, "null sizeofstr");
    unsigned int MAC[8];
    unsigned char CMAC[8];

    SAH_TRACEZ_INFO(ME, "%p %d - %p %d", mac, sizeofmac, str, sizeofstr);

    if(sizeofstr > 16) { /* 12:45:78:ab:de:01 == 17 !*/
        SAH_TRACEZ_INFO(ME, "%s", str);
        if(sscanf(str, "%x:%x:%x:%x:%x:%x",
                  &MAC[0], &MAC[1], &MAC[2],
                  &MAC[3], &MAC[4], &MAC[5]) == 6) {
            CMAC[0] = MAC[0];
            CMAC[1] = MAC[1];
            CMAC[2] = MAC[2];
            CMAC[3] = MAC[3];
            CMAC[4] = MAC[4];
            CMAC[5] = MAC[5];
        } else {
            // No 'str' data was given
            return 0;
        }
    } else {
        if(sizeofstr >= 6) {
            CMAC[0] = str[0];
            CMAC[1] = str[1];
            CMAC[2] = str[2];
            CMAC[3] = str[3];
            CMAC[4] = str[4];
            CMAC[5] = str[5];
        } else {
            // No 'str' data was given
            return 0;
        }
    }
    SAH_TRACEZ_INFO(ME, "%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x",
                    CMAC[0], CMAC[1], CMAC[2], CMAC[3], CMAC[4], CMAC[5]);
    if(sizeofmac >= 6) {
        memcpy(mac, CMAC, 6);
        return 1;
    }

    return 0;
}

bool wldu_convMac2Str(unsigned char* mac, uint32_t sizeofmac, char* str, uint32_t sizeofstr) {
    return swl_hex_fromBytesSep(str, sizeofstr, mac, sizeofmac, false, ':', NULL);
}

static int s_convStrToInt8(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long int res = strtol(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > CHAR_MAX), WLD_ERROR, ME, "num str(%s): CHAR range overflow", numSrcStr);
    ASSERT_FALSE((res < CHAR_MIN), WLD_ERROR, ME, "num str(%s): CHAR range underflow", numSrcStr);
    *((int8_t*) numDstBuf) = (int8_t) res;
    return WLD_OK;
}

static int s_convStrToUInt8(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long int res = strtoul(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > UCHAR_MAX), WLD_ERROR, ME, "num str(%s): UCHAR range overflow", numSrcStr);
    *((uint8_t*) numDstBuf) = (uint8_t) res;
    return WLD_OK;
}

static int s_convStrToInt16(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long int res = strtol(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > SHRT_MAX), WLD_ERROR, ME, "num str(%s): SHORT range overflow", numSrcStr);
    ASSERT_FALSE((res < SHRT_MIN), WLD_ERROR, ME, "num str(%s): SHORT range underflow", numSrcStr);
    *((int16_t*) numDstBuf) = (int16_t) res;
    return WLD_OK;
}

static int s_convStrToUInt16(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long int res = strtoul(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > USHRT_MAX), WLD_ERROR, ME, "num str(%s): USHORT range overflow", numSrcStr);
    *((uint16_t*) numDstBuf) = (uint16_t) res;
    return WLD_OK;
}

static int s_convStrToInt32(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long int res = strtol(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == LONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LONG range overflow", numSrcStr);
    ASSERT_FALSE((res == LONG_MIN) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LONG range underflow", numSrcStr);
    *((int32_t*) numDstBuf) = (int32_t) res;
    return WLD_OK;
}

static int s_convStrToUInt32(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long int res = strtoul(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == ULONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): ULONG range overflow", numSrcStr);
    *((uint32_t*) numDstBuf) = (uint32_t) res;
    return WLD_OK;
}

static int s_convStrToInt64(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long long int res = strtoll(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == LLONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LLONG range overflow", numSrcStr);
    ASSERT_FALSE((res == LLONG_MIN) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LLONG range underflow", numSrcStr);
    *((int64_t*) numDstBuf) = (int64_t) res;
    return WLD_OK;
}

static int s_convStrToUInt64(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long long int res = (uint64_t) strtoull(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == ULLONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): ULLONG range overflow", numSrcStr);
    *((uint64_t*) numDstBuf) = (uint64_t) res;
    return WLD_OK;
}

typedef int (* func_convStrToNum_t)(const char* numSrcStr, void* numDstBuf, uint8_t base);
typedef struct {
    uint8_t byteSize;
    bool isSigned;
    func_convStrToNum_t conv;
} convStrToNum_info_t;
static const convStrToNum_info_t convStrToNumInfo[] = {
    {sizeof(int8_t), true, s_convStrToInt8, },
    {sizeof(uint8_t), false, s_convStrToUInt8, },
    {sizeof(int16_t), true, s_convStrToInt16, },
    {sizeof(uint16_t), false, s_convStrToUInt16, },
    {sizeof(int32_t), true, s_convStrToInt32, },
    {sizeof(uint32_t), false, s_convStrToUInt32, },
    {sizeof(int64_t), true, s_convStrToInt64, },
    {sizeof(uint64_t), false, s_convStrToUInt64, },
};

int wldu_convStrToNum(const char* numSrcStr, void* numDstBuf, uint8_t numDstByteSize, uint8_t base, bool isSigned) {
    ASSERT_NOT_NULL(numSrcStr, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(numDstBuf, WLD_ERROR, ME, "NULL");
    uint32_t i;
    for(i = 0; i < WLD_ARRAY_SIZE(convStrToNumInfo); i++) {
        if((convStrToNumInfo[i].byteSize == numDstByteSize) && (convStrToNumInfo[i].isSigned == isSigned)) {
            return convStrToNumInfo[i].conv(numSrcStr, numDstBuf, base);
        }
    }
    SAH_TRACEZ_ERROR(ME, "unsupp num byte size (%d)", numDstByteSize);
    return WLD_ERROR;
}

/**
 * This function converts a mac address to UPPER case ascii
 */
int mac2str(char* str, const unsigned char* mac, size_t maxstrlen) {
    int len;
    len = snprintf((char*) str, maxstrlen, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if(len < 0) {
        return 0;
    } else if(len <= (signed) maxstrlen) {
        return 1;
    } else {
        return 0;
    }
}

static bool ssid_is_printable(const uint8_t* ssid, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        if(!isprint(ssid[i])) {
            return false;
        }
    }

    return true;
}

/* Convert an SSID (up to 32 bytes, some of which might be 0, not guaranteed to
 * be 0 terminated). into a printable string we can present to the user.
 *
 * If (as is almost always the case) the SSID consists exclusively of printable
 * ASCII characters we copy those. If there are unprintable characters we
 * convert the SSID to a hex string.
 */
char* wld_ssid_to_string(const uint8_t* ssid, uint8_t len) {
    ASSERT_NOT_NULL(ssid, NULL, ME, "NULL");

    /* If the SSID is ASCII printable we just copy that data, if not convert it
     * to a HEX string */
    size_t maxStrSize = 1 + ((ssid_is_printable(ssid, len)) ? len : (len * 2));

    char* str = calloc(1, maxStrSize);
    ASSERT_NOT_NULL(str, str, ME, "NULL");
    convSsid2Str(ssid, len, str, maxStrSize);

    return str;
}

/**
 * @brief Convert the numeric SSID to a readable string
 *
 * This is a direct access function : nothing is allocated
 *
 * @param ssid numeric representation of the ssid
 * @param ssidLen length of the ssid
 * @param str string formatted output of the ssid
 * @param maxStrSize target string buffer size
 * @return 0 on success; -1 on failure
 */
int convSsid2Str(const uint8_t* ssid, uint8_t ssidLen, char* str, size_t maxStrSize) {
    ASSERT_NOT_NULL(ssid, -1, ME, "NULL");

    /* If the SSID is ASCII printable we just copy that data, if not convert it
     * to a HEX string */
    bool ret;
    if(ssid_is_printable(ssid, ssidLen)) {
        ret = swl_str_ncopy(str, maxStrSize, (const char*) ssid, ssidLen);
    } else {
        ret = swl_hex_fromBytes(str, maxStrSize, ssid, ssidLen, false);
    }
    ASSERTI_TRUE(ret, -1, ME, "fail to convert ssid raw to string");

    return 0;
}


/**
 * This function converts the Quality value to a Signal Strength value
 * @date (30/06/2016)
 *
 * @param quality the quality value represented in %
 * @return int the RSSI value represented in dBm
 */
int convQuality2Signal(int quality) {
    int dBm = 0;
    if(quality <= 0) {
        dBm = -100;
    } else if(quality >= 100) {
        dBm = -50;
    } else {
        dBm = (quality / 2) - 100;
    }
    return dBm;
}

/**
 * This function converts the Signal Strength value to a Quality value
 * @date (25/06/2016)
 *
 * @param dBm the RSSI value represented in dBm
 * @return int the quality value represented in %
 */
int convSignal2Quality(int dBm) {
    int quality;
    if(dBm <= -100) {
        quality = 0;
    } else if(dBm >= -50) {
        quality = 100;
    } else {
        quality = 2 * (dBm + 100);
    }
    return quality;
}

/**
 * This function replies on a delayed function callback.
 */
void fsm_delay_reply(uint64_t call_id, amxd_status_t state, int* errval) {
    SAH_TRACEZ_IN(ME);
    amxc_var_t returnValue;
    amxc_var_init(&returnValue);
    amxc_var_set_int32_t(&returnValue, (errval) ? *errval : 0);
    amxd_function_deferred_done(call_id, state, NULL, &returnValue);
    amxc_var_clean(&returnValue);
    SAH_TRACEZ_OUT(ME);
}

int getCountryParam(const char* instr, int CC, int* idx) {
    int i;

    // We're only intrested in the idx value!
    for(i = 0; !instr && CC && Rad_CountryCode[i].shortCountryName; i++) {
        if(CC == Rad_CountryCode[i].countryCode) {
            break;
        }
    }
    // In case we've the short name of the country code...
    for(i = 0; instr && !CC && Rad_CountryCode[i].shortCountryName; i++) {
        if(!strcmp(Rad_CountryCode[i].shortCountryName, instr)) {
            break;
        }
    }

    if(Rad_CountryCode[i].shortCountryName) {
        if(idx) {
            *idx = i;
        }
        if(instr) {
            return Rad_CountryCode[i].countryCode;
        } else {
            return CC;
        }
    }
    // We're not supporting this country...
    return -1;
}

/**
 * The logic is a bit contra... but we must be able to use it
 * for all supported vendor wifi cards (ATH, BCM,...). The
 * common part for all is the Rad_CountryCode table index value!
 */
char* getFullCountryName(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].fullCountryName;
    }
    return NULL;
}

char* getShortCountryName(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].shortCountryName;
    }
    return NULL;
}

int getCountryCode(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].countryCode;
    }
    return 0;
}

swl_opClassCountry_e getCountryZone(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].countryZone;
    }
    return SWL_OP_CLASS_COUNTRY_MAX;
}

/**
 * @details stripOutToken
 *
 * @date (6/28/2012)
 *
 * @param pD Data buffer
 * @param pT Token that must be cut out...
 *
 * return pD pointer!
 */
char* stripOutToken(char* pD, const char* pT) {
    ASSERTS_NOT_NULL(pD, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pT, NULL, ME, "NULL");

    char* pR = pD;
    char* pC = pD;

    while(pR && *pR) {
        if((*pR == *pT) && (pR == strstr(pR, pT))) {
            /* We've a hit */
            pR += strlen(pT);
            continue;
        }
        *pC = *pR;
        pC++;
        pR++;
    }
    *pC = '\0';

    return pD;
}

/**
 *
 * @details stripString
 *
 * @param pTL Buffer for String Token List
 * @param nrTL Max nr of Token List
 * @param pD Data buffer
 * @param pT Token that is used as deliminator
 *
 * @return char**
 */
char** stripString(char** pTL, int nrTL, char* pD, const char* pT) {
    int i;
    char* pCP[2];

    for(i = nrTL - 1; i >= 0; pTL[i] = NULL, i--) {
    }

    i = 0;
    pCP[0] = pD;

    while(pCP[0] && i < nrTL) {
        pTL[i++] = pCP[0];
        pCP[1] = strstr(pCP[0], pT);
        if(pCP[1]) {
            *pCP[1] = '\0';
            pCP[0] = pCP[1] + 1;
        } else {
            return pTL;
        }
    }
    return NULL;
}

/**
 * @brief Convert string notation to hex int
 *
 * @param pbDest the hex int array pointer
 * @param destSize number of hexadicimal values within the int array
 * @param pbSrc the string to convert
 * @param srcSize number of characters within the string
 */
void convStrToHex(uint8_t* pbDest, int destSize, const char* pbSrc, int srcSize) {
    ASSERT_NOT_NULL(pbDest, , ME, "NULL");
    ASSERT_NOT_NULL(pbSrc, , ME, "NULL");
    int hexSize = srcSize / 2;
    ASSERT_TRUE(hexSize <= destSize, , ME, "string to big for small hex array");

    int i;
    for(i = 0; i < hexSize; i++) {
        char h1 = pbSrc[2 * i];
        char h2 = pbSrc[2 * i + 1];

        uint8_t s1 = toupper(h1) - 0x30;
        if(s1 > 9) {
            s1 -= 7;
        }

        uint8_t s2 = toupper(h2) - 0x30;
        if(s2 > 9) {
            s2 -= 7;
        }

        pbDest[i] = s1 * 16 + s2;
    }
}

/**
 * @brief get_pattern_string
 *
 * convert a given hexdigit string based magic pattern to a hex based pattern
 *
 * @param arg hexdigit string based magic pattern
 * @param pattern hex based pattern
 * @return pattern string lenght
 */
int get_pattern_string(const char* arg, uint8_t* pattern) {
    int loop = 0;
    int num = 0;
    int pattern_len = strnlen(arg, MAX_USER_DEFINED_MAGIC << 1);

    while(loop < pattern_len) {
        if(isxdigit(arg[loop]) && isxdigit(arg[loop + 1])) {
            convStrToHex(&pattern[num], 1, &arg[loop], 2);
            num++;
            loop += 2;
        } else {
            loop++;
        }
    }
    return num;
}

int32_t wld_util_performFactorStep(int32_t accum, int32_t newVal, int32_t factor) {
    if(accum == 0) {
        //if currently accumulator is empty, just initialize with newVal
        return newVal * 1000;
    }

    int64_t accumulator = accum;
    accumulator = ((1000 - factor) * accumulator) / 1000 + factor * newVal;
    return (int32_t) accumulator;
}

int wld_writeCapsToString(char* buffer, uint32_t size, const char* const* strArray, uint32_t caps, uint32_t maxCaps) {
    ASSERT_NOT_NULL(buffer, -1, ME, "NULL");
    ASSERT_TRUE(size > 0, -1, ME, "null size");
    uint32_t i = 0;
    uint32_t mask = 0;
    buffer[0] = 0;
    for(i = 0; i < maxCaps; i++) {
        mask = (1 << i);
        if((mask & caps) != 0) {
            swl_strlst_cat(buffer, size, ",", strArray[i]);
            if(strlen(buffer) >= (size - 1)) {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * @brief wld_bitmaskToCSValues
 *
 * Convert string table with bitmask to a comma seperated string list
 *
 * @param string [OUT] comma seperated string list
 * @param stringsize [IN] buffer size of string
 * @param bitmask [IN] bits pattern that represent the supported string values
 * @param strings [IN] string table
 */
void wld_bitmaskToCSValues(char* string, size_t stringsize, const int bitmask, const char* strings[]) {
    int idx, bitchk;
    string[0] = '\0';
    for(idx = 0; strings[idx]; idx++) {
        bitchk = 1 << idx;
        if(bitmask & bitchk) {
            swl_strlst_cat(string, stringsize, ",", strings[idx]);
        }
    }
}

bool wld_util_areAllVapsDisabled(T_Radio* pRad) {
    T_AccessPoint* pAP;
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);
        if((pAP != NULL) && (pAP->pBus != NULL) && pAP->enable) {
            return false;
        }
    }
    return true;
}

bool wld_util_areAllEndpointsDisabled(T_Radio* pRad) {
    T_EndPoint* pEP;
    amxc_llist_for_each(it, &pRad->llEndPoints) {
        pEP = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);
        if(pEP && pEP->enable) {
            return false;
        }
    }
    return true;
}

bool wld_util_isPowerOf(int32_t value, int32_t power) {
    ASSERT_NOT_EQUALS(value, 0, false, ME, "ZERO");
    ASSERT_NOT_EQUALS(power, 0, false, ME, "ZERO");
    while(value != 1) {
        if((value % power) != 0) {
            return false;
        }
        value = value / power;
    }

    return true;
}

bool wld_util_isPowerOf2(int32_t value) {
    ASSERT_TRUE(value > 0, false, ME, "INVALID");
    int bit = value;
    bit &= -bit;
    return (value == (bit));

}

int wld_util_getLinuxIntfState(int socket, char* ifname) {
    return wld_linuxIfUtils_getState(socket, ifname);
}

int wld_util_setLinuxIntfState(int socket, char* ifname, int state) {
    return wld_linuxIfUtils_setState(socket, ifname, state);
}

wld_tuple_t* wldu_getTupleById(wld_tuplelist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        if(list->tuples[i].id == id) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

wld_tupleId_t* wldu_getTupleIdById(wld_tupleIdlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        if(list->tuples[i].id == id) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

wld_tupleVoid_t* wldu_getTupleVoidById(wld_tupleVoidlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        if(list->tuples[i].id == id) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

wld_tuple_t* wldu_getTupleByStr(wld_tuplelist_t* list, char* str) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    ASSERT_NOT_NULL(str, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        ASSERT_NOT_NULL(list->tuples[i].str, NULL, ME, "NULL");
        if(strcmp(list->tuples[i].str, str) == 0) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

const char* wldu_tuple_id2str(wld_tuplelist_t* list, int64_t id, const char* defStr) {
    ASSERT_NOT_NULL(list, defStr, ME, "NULL");
    wld_tuple_t* pTuple = wldu_getTupleById(list, id);
    ASSERTS_NOT_NULL(pTuple, defStr, ME, "NULL");
    return pTuple->str;
}

const void* wldu_tuple_id2void(wld_tupleVoidlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    wld_tupleVoid_t* pTuple = wldu_getTupleVoidById(list, id);
    ASSERTS_NOT_NULL(pTuple, NULL, ME, "NULL");
    return pTuple->ptr;
}

int64_t wldu_tuple_str2id(wld_tuplelist_t* list, char* str, int64_t defId) {
    ASSERT_NOT_NULL(list, defId, ME, "NULL");
    wld_tuple_t* pTuple = wldu_getTupleByStr(list, str);
    ASSERTS_NOT_NULL(pTuple, defId, ME, "NULL");
    return pTuple->id;
}

int64_t wldu_tuple_id2index(wld_tupleIdlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, 0, ME, "NULL");
    wld_tupleId_t* pTuple = wldu_getTupleIdById(list, id);
    ASSERTS_NOT_NULL(pTuple, 0, ME, "NULL");
    return pTuple->index;
}

/*
 * converts a string to a mask based on tuple set, where tuples contain masks.
 * return false if not all the tuple set is matched.
 */
bool wldu_tuple_convStrToMaskByMask(wld_tuplelist_t* pList, const amxc_string_t* pNames, uint64_t* pBitMask) {
    bool allMatched = true;
    uint64_t bitMask = 0;
    ASSERT_NOT_NULL(pNames, false, ME, "NULL");
    ASSERT_NOT_NULL(pList, false, ME, "NULL");
    if(pBitMask) {
        *pBitMask = 0;
    }
    ASSERTS_FALSE(amxc_string_is_empty(pNames), true, ME, "empty names selection");
    ASSERTS_NOT_EQUALS(pList->size, 0, false, ME, "empty list");
    wld_tuple_t* pTuple;
    char selection[amxc_string_text_length(pNames) + 1];
    swl_str_copy(selection, sizeof(selection), pNames->buffer);
    char* p = selection;
    char* tk = NULL;
    while((tk = strsep(&p, ",")) != NULL) {
        if(*tk == 0) {
            continue;
        }
        pTuple = wldu_getTupleByStr(pList, tk);
        if(pTuple == NULL) {
            SAH_TRACEZ_NOTICE(ME, "unknow name (%s)", tk);
            allMatched = false;
            continue;
        }
        bitMask |= pTuple->id;
    }
    if(pBitMask) {
        *pBitMask = bitMask;
    }
    return allMatched;
}

/*
 * converts a bitmask to a string based on tuple set, where tuples contain masks.
 * return false if not all the tuple set is matched.
 */
bool wldu_tuple_convMaskToStrByMask(wld_tuplelist_t* pList, uint64_t bitMask, amxc_string_t* pNames) {
    bool allMatched = true;
    ASSERT_NOT_NULL(pList, false, ME, "NULL");
    ASSERT_NOT_NULL(pNames, false, ME, "NULL");
    uint32_t i;
    int64_t id;
    uint64_t flag;
    wld_tuple_t* pTuple;
    amxc_string_clean(pNames);
    ASSERTS_NOT_EQUALS(bitMask, 0, true, ME, "empty bitmask selection");
    ASSERTS_NOT_EQUALS(pList->size, 0, false, ME, "empty list");
    for(i = 0; i < WLD_BIT_SIZE(bitMask); i++) {
        flag = (1ULL << i);
        id = (bitMask & flag);
        if(id == 0) {
            continue;
        }
        pTuple = wldu_getTupleById(pList, id);
        if(pTuple == NULL) {
            SAH_TRACEZ_NOTICE(ME, "unknown bitmask flag at shifted pos %d", i);
            allMatched = false;
            continue;
        }
        amxc_string_appendf(pNames, "%s%s", amxc_string_is_empty(pNames) ? "" : ",", pTuple->str);
    }
    return allMatched;
}
/*
 *   // Function to validate if the path is safe
 */
static bool s_isValidAbsolutePath(const char* path) {
    return (swl_str_startsWith(path, "/") && (swl_str_find(path, "../") < 0));
}
char* wldu_getLocalFile(char* buffer, size_t bufSize, char* path, char* format, ...) {
    swl_str_copy(buffer, bufSize, NULL);
    ASSERT_STR(format, NULL, ME, "empty format");
    va_list args;
    char* basePath = getenv("WLD_DBG_PATH");

    char fileNameBuffer[128];
    va_start(args, format);
    int ret = vsnprintf(fileNameBuffer, sizeof(fileNameBuffer), format, args);
    va_end(args);
    ASSERT_TRUE(ret >= 0, NULL, ME, "Invalid format / args %s", format);
    if(!swl_str_isEmpty(basePath)) {
        if(!s_isValidAbsolutePath(basePath)) {
            SAH_TRACEZ_WARNING(ME, "ignore unsafe dbg basepath (%s)", basePath);
        } else if(!swl_str_catFormat(buffer, bufSize, "%s/%s", basePath, fileNameBuffer)) {
            SAH_TRACEZ_WARNING(ME, "ignore dbg basepath (%s): tgt buffer(%p) too short (%zu)", basePath, buffer, bufSize);
        } else if(access(buffer, F_OK) != -1) {
            return buffer;
        } else {
            SAH_TRACEZ_NOTICE(ME, "file (%s) not found in dbg basePath (%s): fallback to use arg path(%s)", fileNameBuffer, basePath, path);
        }
        swl_str_copy(buffer, bufSize, NULL);
    }
    if(!swl_str_isEmpty(path)) {
        swl_str_catFormat(buffer, bufSize, "%s/", path);
    }
    swl_str_cat(buffer, bufSize, fileNameBuffer);
    return buffer;
}

swl_bandwidth_e wld_util_getMaxBwCap(wld_assocDev_capabilities_t* caps) {
    if(caps->ehtCapabilities & M_SWL_STACAP_EHT_320MHZ_6GHZ) {
        return SWL_BW_320MHZ;
    }
    if(caps->vhtCapabilities & M_SWL_STACAP_VHT_SGI160) {
        return SWL_BW_160MHZ;
    }
    if(caps->vhtCapabilities & M_SWL_STACAP_VHT_SGI80) {
        return SWL_BW_80MHZ;
    }
    if(caps->htCapabilities & M_SWL_STACAP_HT_SGI40) {
        return SWL_BW_40MHZ;
    }
    if(caps->htCapabilities & M_SWL_STACAP_HT_SGI20) {
        return SWL_BW_20MHZ;
    }
    return SWL_BW_AUTO;
}
/**
 * Create a new MD5 hash string based on ssid and key string.
 */
void wldu_convCreds2MD5(const char* ssid, const char* key, char* md5, size_t md5_size) {
    char newKeyPassPhrase_md5[PSK_KEY_SIZE_LEN - 1] = {'\0'};
    PKCS5_PBKDF2_HMAC_SHA1(key, strlen(key),
                           (const unsigned char*) ssid, strlen(ssid), 4096,
                           sizeof(newKeyPassPhrase_md5), (unsigned char*) newKeyPassPhrase_md5);

    bool result = swl_hex_fromBytes(md5, md5_size, (const swl_bit8_t*) newKeyPassPhrase_md5, sizeof(newKeyPassPhrase_md5), false);

    if(!result) {
        md5[0] = '\0';
    }
}

/**
 * Compare current key with new key.
 * If one key is a hash, a new hash is created and compared.
 * return false if different, true if equals.
 */
bool wldu_key_matches(const char* ssid, const char* oldKeyPassPhrase, const char* keyPassPhrase) {
    ASSERTS_NOT_NULL(ssid, false, ME, "NULL");
    ASSERTS_NOT_NULL(oldKeyPassPhrase, false, ME, "NULL");
    ASSERTS_NOT_NULL(keyPassPhrase, false, ME, "NULL");
    if(!strcmp(oldKeyPassPhrase, keyPassPhrase)) {
        SAH_TRACEZ_INFO(ME, "Keys are identical");
        return true;
    }
    int newKeyLen = strlen(keyPassPhrase) + 1;
    int currentKeyLen = strlen(oldKeyPassPhrase) + 1;
    char newKeyPassPhrase_md5_str[PSK_KEY_SIZE_LEN * 2 - 1] = {'\0'};
    if((currentKeyLen == PSK_KEY_SIZE_LEN) && (newKeyLen != PSK_KEY_SIZE_LEN)) {
        //oldKeyPassPhrase is a hash, need to compare this to the new hash
        wldu_convCreds2MD5(ssid, keyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN * 2 - 1);
        return (strncmp(oldKeyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN - 1) == 0);
    } else if((currentKeyLen != PSK_KEY_SIZE_LEN) && (newKeyLen == PSK_KEY_SIZE_LEN)) {
        //oldKeyPassPhrase is not a hash and keyPassPhrase is a hash
        wldu_convCreds2MD5(ssid, oldKeyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN * 2 - 1);
        return (strncmp(keyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN - 1) == 0);
    } else {
        SAH_TRACEZ_INFO(ME, "Keys are different");
        return false;
    }
    SAH_TRACEZ_INFO(ME, "Keys are identical");
    return true;
}

static void s_writeWmmStats(amxd_object_t* parentObj, const char* objectName, uint32_t* stats) {
    amxd_object_t* object = amxd_object_get(parentObj, objectName);
    ASSERT_NOT_NULL(object, , ME, "No object named <%s>", objectName);
    SWLA_OBJECT_SET_PARAM_UINT32(object, "AC_BE", stats[WLD_AC_BE]);
    SWLA_OBJECT_SET_PARAM_UINT32(object, "AC_BK", stats[WLD_AC_BK]);
    SWLA_OBJECT_SET_PARAM_UINT32(object, "AC_VO", stats[WLD_AC_VO]);
    SWLA_OBJECT_SET_PARAM_UINT32(object, "AC_VI", stats[WLD_AC_VI]);
}

amxd_status_t wld_util_stats2Obj(amxd_object_t* obj, T_Stats* stats) {
    ASSERT_NOT_NULL(obj, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(stats, amxd_status_unknown_error, ME, "NULL");

    SWLA_OBJECT_SET_PARAM_UINT64(obj, "BytesSent", stats->BytesSent);
    SWLA_OBJECT_SET_PARAM_UINT64(obj, "BytesReceived", stats->BytesReceived);
    SWLA_OBJECT_SET_PARAM_UINT64(obj, "PacketsSent", stats->PacketsSent);
    SWLA_OBJECT_SET_PARAM_UINT64(obj, "PacketsReceived", stats->PacketsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "ErrorsSent", stats->ErrorsSent);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "RetransCount", stats->RetransCount);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "ErrorsReceived", stats->ErrorsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "DiscardPacketsSent", stats->DiscardPacketsSent);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "DiscardPacketsReceived", stats->DiscardPacketsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnicastPacketsSent", stats->UnicastPacketsSent);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnicastPacketsReceived", stats->UnicastPacketsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MulticastPacketsSent", stats->MulticastPacketsSent);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MulticastPacketsReceived", stats->MulticastPacketsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "BroadcastPacketsSent", stats->BroadcastPacketsSent);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "BroadcastPacketsReceived", stats->BroadcastPacketsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnknownProtoPacketsReceived", stats->UnknownProtoPacketsReceived);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "FailedRetransCount", stats->FailedRetransCount);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "RetryCount", stats->RetryCount);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MultipleRetryCount", stats->MultipleRetryCount);
    SWLA_OBJECT_SET_PARAM_INT32(obj, "Temperature", stats->TemperatureDegreesCelsius);
    SWLA_OBJECT_SET_PARAM_INT32(obj, "Noise", stats->noise);

    s_writeWmmStats(obj, "WmmPacketsSent", stats->WmmPacketsSent);
    s_writeWmmStats(obj, "WmmPacketsReceived", stats->WmmPacketsReceived);
    s_writeWmmStats(obj, "WmmFailedSent", stats->WmmFailedSent);
    s_writeWmmStats(obj, "WmmFailedReceived", stats->WmmFailedReceived);
    s_writeWmmStats(obj, "WmmBytesSent", stats->WmmBytesSent);
    s_writeWmmStats(obj, "WmmFailedbytesSent", stats->WmmFailedBytesSent);
    s_writeWmmStats(obj, "WmmBytesReceived", stats->WmmBytesReceived);
    s_writeWmmStats(obj, "WmmFailedBytesReceived", stats->WmmFailedBytesReceived);

    return amxd_status_ok;
}

amxd_status_t wld_util_statsObj2Var(amxc_var_t* map, amxd_object_t* statsObj) {
    amxd_status_t status = swla_object_paramsToMap(map, statsObj);
    ASSERTS_EQUALS(status, amxd_status_ok, status, ME, "fail dump stat params");
    swla_object_addChildParamsToMap(map, statsObj, "WmmPacketsSent");
    swla_object_addChildParamsToMap(map, statsObj, "WmmPacketsReceived");
    swla_object_addChildParamsToMap(map, statsObj, "WmmFailedSent");
    swla_object_addChildParamsToMap(map, statsObj, "WmmFailedReceived");
    swla_object_addChildParamsToMap(map, statsObj, "WmmBytesSent");
    swla_object_addChildParamsToMap(map, statsObj, "WmmFailedbytesSent");
    swla_object_addChildParamsToMap(map, statsObj, "WmmBytesReceived");
    swla_object_addChildParamsToMap(map, statsObj, "WmmFailedBytesReceived");
    return status;
}

void wld_util_accumulateStats(T_Stats* pAccStats, T_Stats* pDiffStats) {
    ASSERT_NOT_NULL(pAccStats, , ME, "NULL");
    ASSERT_NOT_NULL(pDiffStats, , ME, "NULL");

    pAccStats->BytesSent += pDiffStats->BytesSent;
    pAccStats->BytesReceived += pDiffStats->BytesReceived;
    pAccStats->PacketsSent += pDiffStats->PacketsSent;
    pAccStats->PacketsReceived += pDiffStats->PacketsReceived;
    pAccStats->ErrorsSent += pDiffStats->ErrorsSent;
    pAccStats->ErrorsReceived += pDiffStats->ErrorsReceived;
    pAccStats->RetransCount += pDiffStats->RetransCount;
    pAccStats->DiscardPacketsSent += pDiffStats->DiscardPacketsSent;
    pAccStats->DiscardPacketsReceived += pDiffStats->DiscardPacketsReceived;
    pAccStats->UnicastPacketsSent += pDiffStats->UnicastPacketsSent;
    pAccStats->UnicastPacketsReceived += pDiffStats->UnicastPacketsReceived;
    pAccStats->MulticastPacketsSent += pDiffStats->MulticastPacketsSent;
    pAccStats->MulticastPacketsReceived += pDiffStats->MulticastPacketsReceived;
    pAccStats->BroadcastPacketsSent += pDiffStats->BroadcastPacketsSent;
    pAccStats->BroadcastPacketsReceived += pDiffStats->BroadcastPacketsReceived;
    pAccStats->UnknownProtoPacketsReceived += pDiffStats->UnknownProtoPacketsReceived;
    pAccStats->FailedRetransCount += pDiffStats->FailedRetransCount;
    pAccStats->RetryCount += pDiffStats->RetryCount;
    pAccStats->MultipleRetryCount += pDiffStats->MultipleRetryCount;
}

void wld_util_updateStatusChangeInfo(wld_status_changeInfo_t* info, wld_status_e status) {
    ASSERT_NOT_NULL(info, , ME, "NULL");
    swl_timeMono_t now = swl_time_getMonoSec();
    if(info->lastStatusHistogramUpdate == 0) {
        info->lastStatusHistogramUpdate = now;
        return;
    }

    swl_timeMono_t timeDiff = now - info->lastStatusHistogramUpdate;
    info->statusHistogram.data[status] += timeDiff;
    info->lastStatusHistogramUpdate = now;
}

static char sReferencePathPrefixStr[128] = ROOT_OBJ_PREFIX_STR;

const char* wld_util_getReferencePathPrefix() {
    return sReferencePathPrefixStr;
}

bool wld_util_setReferencePathPrefix(const char* prefix) {
    return swl_str_copy(sReferencePathPrefixStr, sizeof(sReferencePathPrefixStr), prefix);
}

bool wld_util_addReferencePathPrefix(char* outRefPath, size_t outRefPathSize, const char* currRefPath) {
    const char* refPrefix = wld_util_getReferencePathPrefix();
    char outBuf[swl_str_len(currRefPath) + swl_str_len(refPrefix) + 2];
    outBuf[0] = 0;
    if(!swl_str_isEmpty(refPrefix) && !swl_str_startsWith(currRefPath, refPrefix)) {
        swl_str_copy(outBuf, sizeof(outBuf), refPrefix);
    }
    swl_str_cat(outBuf, sizeof(outBuf), currRefPath);
    return swl_str_copy(outRefPath, outRefPathSize, outBuf);
}

swl_rc_ne wld_util_getRealReferencePath(char* outRefPath, size_t outRefPathSize, const char* currRefPath, amxd_object_t* currRefObj) {
    ASSERT_NOT_NULL(outRefPath, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(outRefPathSize > 0, SWL_RC_INVALID_PARAM, ME, "wrong size");
    outRefPath[0] = 0;

    amxd_object_t* refObj = swla_object_getReferenceObject(get_wld_object(), currRefPath);
    amxd_object_type_t pathType = amxd_object_get_type(refObj);
    amxd_object_type_t objType = amxd_object_get_type(currRefObj);

    if((objType != amxd_object_instance) || (refObj == currRefObj)) {
        ASSERT_EQUALS(pathType, amxd_object_instance, SWL_RC_ERROR, ME, "No possible reference to instance object");
        // searched object is a valid instance: then keep the current path format as provided
        swl_str_copy(outRefPath, outRefPathSize, currRefPath);
    } else {
        // keep the indexed format path of current referenced object
        char* path = amxd_object_get_path(currRefObj, AMXD_OBJECT_INDEXED);
        swl_str_copy(outRefPath, outRefPathSize, path);
        free(path);
    }
    if(!swl_str_isEmpty(outRefPath) && (outRefPath[strlen(outRefPath) - 1] != '.')) {
        swl_str_cat(outRefPath, outRefPathSize, ".");
    }
    bool ret = wld_util_addReferencePathPrefix(outRefPath, outRefPathSize, outRefPath);
    return ret ? SWL_RC_OK : SWL_RC_ERROR;
}

swl_rc_ne wld_util_getManagementFrameParameters(T_Radio* pRad, wld_util_managementFrame_t* mgmtFrame, amxc_var_t* args) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(mgmtFrame, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(args, SWL_RC_INVALID_PARAM, ME, "NULL");

    const char* macStr = GET_CHAR(args, "mac");
    bool ok = SWL_MAC_CHAR_TO_BIN(&mgmtFrame->mac, macStr);
    ASSERTS_TRUE(ok, SWL_RC_INVALID_PARAM, ME, "Fail to convert mac");

    const char* frameControlStr = GET_CHAR(args, "fc");
    ASSERT_NOT_NULL(frameControlStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    size_t frameControlStrLen = swl_str_len(frameControlStr) / 2;
    ASSERT_EQUALS(frameControlStrLen, 2, SWL_RC_INVALID_PARAM, ME, "Invalid frame control size");
    bool success = swl_hex_toBytes((swl_bit8_t*) &mgmtFrame->fc, (sizeof(swl_80211_mgmtFrameControl_t) + 1), frameControlStr, frameControlStrLen);
    ASSERTS_TRUE(success, SWL_RC_INVALID_PARAM, ME, "hex to bytes error");

    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(GET_UINT32(args, "channel"), SWL_BW_20MHZ, pRad->operatingFrequencyBand);
    if(chanspec.channel == 0) {
        chanspec.channel = pRad->currentChanspec.chanspec.channel;
    }
    memcpy(&mgmtFrame->chanspec, &chanspec, sizeof(swl_chanspec_t));

    const char* data = GET_CHAR(args, "data");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");
    size_t dataLen = swl_str_len(data);
    mgmtFrame->dataLen = (dataLen / 2) + (dataLen % 2);
    mgmtFrame->data = calloc(1, mgmtFrame->dataLen);
    ASSERT_NOT_NULL(mgmtFrame->data, SWL_RC_INVALID_PARAM, ME, "NULL");
    success = swl_hex_toBytes(mgmtFrame->data, mgmtFrame->dataLen + 1, data, dataLen);
    if(!success) {
        free(mgmtFrame->data);
        return SWL_RC_INVALID_PARAM;
    }

    return SWL_RC_OK;
}

uint32_t wld_util_countScanResultEntriesPerChannel(wld_scanResults_t* results, swl_channel_t channel) {
    ASSERTS_NOT_NULL(results, 0, ME, "NULL");
    uint32_t count = 0;
    amxc_llist_for_each(it, &results->ssids) {
        wld_scanResultSSID_t* pNeighBss = amxc_container_of(it, wld_scanResultSSID_t, it);
        if(pNeighBss->channel != (int32_t) channel) {
            continue;
        }
        count++;
    }
    return count;
}

void wld_util_initCustomAlias(amxd_trans_t* trans, amxd_object_t* object) {
    ASSERTS_NOT_NULL(trans, , ME, "NULL");
    ASSERTS_NOT_NULL(object, , ME, "NULL");
    char* customAlias = amxd_object_get_cstring_t(object, "CustomAlias", NULL);
    if((customAlias != NULL) && swl_str_isEmpty(customAlias)) {
        amxd_trans_set_cstring_t(trans, "CustomAlias", amxd_object_get_name(object, AMXD_OBJECT_NAMED));
    }
    free(customAlias);
}

/*
 * @brief return previous object instance
 */
amxd_object_t* wld_util_getPrevObjInst(amxd_object_t* instance) {
    ASSERTS_EQUALS(amxd_object_get_type(instance), amxd_object_instance, NULL, ME, "Not instance");
    amxc_llist_it_t* it = amxc_llist_it_get_previous(&instance->it);
    ASSERTS_NOT_NULL(it, NULL, ME, "No previous iteration");
    return amxc_container_of(it, amxd_object_t, it);
}

/*
 * @brief return next object instance
 */
amxd_object_t* wld_util_getNextObjInst(amxd_object_t* instance) {
    ASSERTS_EQUALS(amxd_object_get_type(instance), amxd_object_instance, NULL, ME, "Not instance");
    amxc_llist_it_t* it = amxc_llist_it_get_next(&instance->it);
    ASSERTS_NOT_NULL(it, NULL, ME, "No next iteration");
    return amxc_container_of(it, amxd_object_t, it);
}

wld_spectrumChannelInfoEntry_t* wld_util_getSpectrumEntryByChannel(amxc_llist_t* pSpectrumInfoList, swl_channel_t channel) {
    ASSERTS_NOT_NULL(pSpectrumInfoList, NULL, ME, "NULL");
    amxc_llist_for_each(it, pSpectrumInfoList) {
        wld_spectrumChannelInfoEntry_t* pEntry = amxc_container_of(it, wld_spectrumChannelInfoEntry_t, it);
        if(pEntry->channel == channel) {
            return pEntry;
        }
    }
    return NULL;
}

wld_spectrumChannelInfoEntry_t* wld_util_addorUpdateSpectrumEntry(amxc_llist_t* llSpectrumChannelInfo, wld_spectrumChannelInfoEntry_t* pData) {
    ASSERT_NOT_NULL(llSpectrumChannelInfo, NULL, ME, "NULL");
    ASSERT_NOT_NULL(pData, NULL, ME, "NULL");
    ASSERTI_TRUE(pData->channel > 0, NULL, ME, "invalid channel");
    bool isNew = false;
    wld_spectrumChannelInfoEntry_t* pEntry = wld_util_getSpectrumEntryByChannel(llSpectrumChannelInfo, pData->channel);
    if(pEntry == NULL) {
        pEntry = calloc(1, sizeof(wld_spectrumChannelInfoEntry_t));
        ASSERT_NOT_NULL(pEntry, NULL, ME, "Fail to allocate entry");
        isNew = true;
    }
    amxc_llist_it_t it = pEntry->it;
    memcpy(pEntry, pData, sizeof(wld_spectrumChannelInfoEntry_t));
    if(isNew) {
        amxc_llist_it_init(&pEntry->it);
        amxc_llist_append(llSpectrumChannelInfo, &pEntry->it);
    } else {
        pEntry->it = it;
    }
    return pEntry;
}

swl_rc_ne wld_util_fetchExecutablePath(const char* searchPaths, const char* cmd, char* buf, size_t bufSize) {
    ASSERT_STR(searchPaths, SWL_RC_INVALID_PARAM, ME, "Empty searchPaths");
    ASSERT_STR(cmd, SWL_RC_INVALID_PARAM, ME, "Empty command");
    char tmpSPath[swl_str_len(searchPaths) + 1];
    swl_str_copy(tmpSPath, sizeof(tmpSPath), searchPaths);
    char* p = tmpSPath;
    char* tk = NULL;
    while((tk = strsep(&p, ":")) != NULL) {
        //skip empty fields
        if(*tk == 0) {
            continue;
        }
        char fullPath[swl_str_len(tk) + swl_str_len(cmd) + 2];
        snprintf(fullPath, sizeof(fullPath), "%s%s%s",
                 tk, ((tk[swl_str_len(tk) - 1] == '/') ? "" : "/"), cmd);
        char resPath[PATH_MAX] = {0};
        if(realpath(fullPath, resPath) == NULL) {
            continue;
        }
        struct stat sb;
        memset(&sb, 0, sizeof(sb));
        if(stat(resPath, &sb) != 0) {
            SAH_TRACEZ_WARNING(ME, "fail to get info of (%s)", resPath);
            continue;
        }
        if(S_ISREG(sb.st_mode) && (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            SAH_TRACEZ_INFO(ME, "found real cmd(%s) exec file in (%s)", cmd, resPath);
            return swl_str_copy(buf, bufSize, resPath) ? SWL_RC_OK : SWL_RC_RESULT_OUT_OF_BOUNDS;
        }
    }
    SAH_TRACEZ_WARNING(ME, "not found cmd(%s) exec bin in (%s)", cmd, searchPaths);
    return SWL_RC_NOT_FOUND;
}

swl_rc_ne wld_util_getExecutablePath(const char* cmd, char* buf, size_t bufSize) {
    return wld_util_fetchExecutablePath(getenv("PATH"), cmd, buf, bufSize);
}

swl_rc_ne wld_util_copyScanInfoFromIEs(wld_scanResultSSID_t* pResult, swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    ASSERTS_NOT_NULL(pResult, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWirelessDevIE, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(pWirelessDevIE->operChanInfo.channel > 0) {
        pResult->channel = pWirelessDevIE->operChanInfo.channel;
    }
    if(pWirelessDevIE->operChanInfo.bandwidth != SWL_BW_AUTO) {
        pResult->bandwidth = swl_chanspec_bwToInt(pWirelessDevIE->operChanInfo.bandwidth);
    }
    swl_chanspec_t chanSpec = SWL_CHANSPEC_NEW(pResult->channel, pWirelessDevIE->operChanInfo.bandwidth, pWirelessDevIE->operChanInfo.band);
    pResult->centreChannel = swl_chanspec_getCentreChannel(&chanSpec);
    swl_operatingClass_t operClass = swl_chanspec_getOperClass(&chanSpec);
    if(operClass > 0) {
        pResult->operClass = operClass;
    }
    pResult->ssidLen = SWL_MIN((uint8_t) sizeof(pResult->ssid), pWirelessDevIE->ssidLen);
    memcpy(pResult->ssid, pWirelessDevIE->ssid, pResult->ssidLen);
    pResult->operatingStandards = pWirelessDevIE->operatingStandards;
    pResult->secModeEnabled = pWirelessDevIE->secModeEnabled;
    pResult->WPS_ConfigMethodsEnabled = pWirelessDevIE->WPS_ConfigMethodsEnabled;
    return SWL_RC_OK;
}

swl_80211_ehtOpIE_t wld_util_buildEhtOperationIE(swl_chanspec_t tgtChspec, swl_bit32_t bitmap, uint32_t nTx, uint32_t nRx) {
    swl_80211_ehtOpIE_t ehtOperationIE;
    memset(&ehtOperationIE, 0, sizeof(ehtOperationIE));

    ehtOperationIE.eht_operation_information_present = 1;
    swl_channel_t centerChan = swl_chanspec_getCentreChannel(&tgtChspec);
    swl_bandwidth_e tgtChW = tgtChspec.bandwidth;
    switch(tgtChW) {
    case SWL_BW_40MHZ:
        ehtOperationIE.ehtOpInfo.control_channel_width = 1;
        ehtOperationIE.ehtOpInfo.ccfs0 = centerChan;
        break;
    case SWL_BW_80MHZ:
        ehtOperationIE.ehtOpInfo.control_channel_width = 2;
        ehtOperationIE.ehtOpInfo.ccfs0 = centerChan;
        break;
    case SWL_BW_160MHZ:
        ehtOperationIE.ehtOpInfo.control_channel_width = 3;
        ehtOperationIE.ehtOpInfo.ccfs1 = centerChan;
        /*
         * For 160 MHz BSS bandwidth, ccfs0 indicates the channel
         * center frequency index of the primary 80 MHz channel.
         */
        swl_chanspec_t lowerChspec = tgtChspec;
        lowerChspec.bandwidth = SWL_BW_80MHZ;
        ehtOperationIE.ehtOpInfo.ccfs0 = swl_chanspec_getCentreChannel(&lowerChspec);
        break;
    case SWL_BW_320MHZ:
        ehtOperationIE.ehtOpInfo.control_channel_width = 4;
        ehtOperationIE.ehtOpInfo.ccfs1 = centerChan;
        /*
         * For 320 MHz BSS bandwidth, indicates the channel
         * center frequency index of the primary 160 MHz channel.
         */
        lowerChspec = tgtChspec;
        lowerChspec.bandwidth = SWL_BW_160MHZ;
        ehtOperationIE.ehtOpInfo.ccfs0 = swl_chanspec_getCentreChannel(&lowerChspec);
        break;
    case SWL_BW_20MHZ:
    default:
        ehtOperationIE.ehtOpInfo.control_channel_width = 0;
        ehtOperationIE.ehtOpInfo.ccfs0 = centerChan;
        break;
    }

    SAH_TRACEZ_INFO(ME, "ccfs0 is %u", ehtOperationIE.ehtOpInfo.ccfs0);
    SAH_TRACEZ_INFO(ME, "ccfs1 is %u", ehtOperationIE.ehtOpInfo.ccfs1);
    SAH_TRACEZ_INFO(ME, "control_channel_width is %u", ehtOperationIE.ehtOpInfo.control_channel_width);

    if(bitmap) {
        ehtOperationIE.disabled_subchannel_bitmap_present = 1;
    }
    ehtOperationIE.ehtOpInfo.disabled_sub_channel_bitmap = bitmap;
    SAH_TRACEZ_INFO(ME, "bitmap is %u", ehtOperationIE.ehtOpInfo.disabled_sub_channel_bitmap);

    uint32_t byte = (uint32_t) (nTx & 0x0F) << 4 | (nRx & 0x0F);
    ehtOperationIE.basic_eht_mcs_n_nss_set = byte * 0x01010101;
    SAH_TRACEZ_INFO(ME, "basic eht mcs and nss set is 0x%X", ehtOperationIE.basic_eht_mcs_n_nss_set);

    return ehtOperationIE;
}

