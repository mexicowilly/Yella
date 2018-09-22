/*
 * Copyright 2016 Will Mason
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "agent/mac_addresses.h"
#include <chucho/log.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>

static bool find_ethernet_interfaces(io_iterator_t* matchingServices, chucho_logger_t* lgr)
{
    kern_return_t kernResult;
    CFMutableDictionaryRef matchingDict;
    CFMutableDictionaryRef propertyMatchDict;

    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass.
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and
    // the specified value.
    matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);

    if (matchingDict == NULL)
    {
        CHUCHO_C_ERROR_L(lgr, "Could not look up ethernet addresses (IOServiceMatching)");
        return false;
    }
    else
    {
        // Each IONetworkInterface object has a Boolean property with the key kIOPrimaryInterface. Only the
        // primary (built-in) interface has this property set to TRUE.

        // IOServiceGetMatchingServices uses the default matching criteria defined by IOService. This considers
        // only the following properties plus any family-specific matching in this order of precedence
        // (see IOService::passiveMatch):
        //
        // kIOProviderClassKey (IOServiceMatching)
        // kIONameMatchKey (IOServiceNameMatching)
        // kIOPropertyMatchKey
        // kIOPathMatchKey
        // kIOMatchedServiceCountKey
        // family-specific matching
        // kIOBSDNameKey (IOBSDNameMatching)
        // kIOLocationMatchKey

        // The IONetworkingFamily does not define any family-specific matching. This means that in
        // order to have IOServiceGetMatchingServices consider the kIOPrimaryInterface property, we must
        // add that property to a separate dictionary and then add that to our matching dictionary
        // specifying kIOPropertyMatchKey.

        propertyMatchDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks);

        if (propertyMatchDict == NULL)
        {
            CHUCHO_C_ERROR_L(lgr, "Could not look up ethernet addresses (CFDictionaryCreateMutable)");
        }
        else
        {
            // Set the value in the dictionary of the property with the given key, or add the key
            // to the dictionary if it doesn't exist. This call retains the value object passed in.
            CFDictionarySetValue(propertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);

            // Now add the dictionary containing the matching value for kIOPrimaryInterface to our main
            // matching dictionary. This call will retain propertyMatchDict, so we can release our reference
            // on propertyMatchDict after adding it to matchingDict.
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
            CFRelease(propertyMatchDict);
        }
    }

    // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
    // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
    // the dictionary explicitly.
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, matchingServices);
    if (kernResult != KERN_SUCCESS)
    {
        CHUCHO_C_ERROR_L(lgr, "Could not look up ethernet addresses (IOServiceGetMatchingServices)");
        return false;
    }

    return true;
}

yella_mac_addresses* yella_get_mac_addresses(chucho_logger_t* lgr)
{
    io_object_t service;
    io_object_t controllerService;
    kern_return_t kernResult = KERN_FAILURE;
    size_t capacity = 20;
    yella_mac_addresses* result = calloc(1, sizeof(yella_mac_addresses));
    io_iterator_t itor;
    yella_mac_address* tmp;
    uint8_t* cur_addr;

    if (find_ethernet_interfaces(&itor, lgr))
    {
        result->addrs = malloc(capacity * sizeof(yella_mac_address));
        service = IOIteratorNext(itor);
        while (service != 0)
        {
            CFTypeRef MACAddressAsCFData;

            // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call,
            // since they are hardware nubs and do not participate in driver matching. In other words,
            // registerService() is never called on them. So we've found the IONetworkInterface and will
            // get its parent controller by asking for it specifically.

            // IORegistryEntryGetParentEntry retains the returned object, so release it when we're done with it.
            kernResult = IORegistryEntryGetParentEntry(service,
                                                       kIOServicePlane,
                                                       &controllerService);

            if (kernResult != KERN_SUCCESS)
            {
                CHUCHO_C_ERROR_L(lgr, "Error getting interface parent entry for service %i", service);
            }
            else
            {
                // Retrieve the MAC address property from the I/O Registry in the form of a CFData
                MACAddressAsCFData = IORegistryEntryCreateCFProperty(controllerService,
                                                                     CFSTR(kIOMACAddress),
                                                                     kCFAllocatorDefault,
                                                                     0);
                if (MACAddressAsCFData)
                {
                    // Get the raw bytes of the MAC address from the CFData
                    cur_addr = result->addrs[result->count].addr;
                    CFDataGetBytes(MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), cur_addr);
                    CFRelease(MACAddressAsCFData);
                    snprintf(result->addrs[result->count].text,
                             sizeof(result->addrs[result->count++].text),
                             "%02x:%02x:%02x:%02x:%02x:%02x",
                             cur_addr[0], cur_addr[1], cur_addr[2], cur_addr[3], cur_addr[4], cur_addr[5]);
                    if (++result->count == capacity)
                    {
                        capacity *= 2;
                        tmp = malloc(capacity * sizeof(yella_mac_address));
                        memcpy(tmp, result->addrs, result->count * sizeof(yella_mac_address));
                        free(result->addrs);
                        result->addrs = tmp;
                    }
                }

                // Done with the parent Ethernet controller object so we release it.
                IOObjectRelease(controllerService);
            }

            // Done with the Ethernet interface object so we release it.
            IOObjectRelease(service);
            service = IOIteratorNext(itor);
        }
        if (result->addrs != NULL)
            result->addrs = realloc(result->addrs, result->count * sizeof(yella_mac_address));
    }

    return result;
}
