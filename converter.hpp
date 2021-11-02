/*!
 * @file converter.hpp
 * @brief The main converter tool header
 *
 * This header specifies the functions of the SDF/YANG converter tool.
 */

#ifndef CONVERTER_H
#define CONVERTER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <cstring>
#include <regex>
#include <memory>
#include <math.h>
#include <ctype.h>
#include <algorithm>
#include <libyang/libyang.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <dirent.h>
#include <limits>
#include "sdf.hpp"

//#define MAX_NUM 3.4e+38                
#define MAX_NUM numeric_limits<float>::max() /**< Maximal value for float numbers
                                        *   (needed for YANG's special min and
                                        *   max values)
                                        */
//#define MAX_INT 9223372036854775807    
#define MAX_INT numeric_limits<int>::max() /**< Maximal value for int64 numbers
                                        *   (needed for YANG's special min and
                                        *   max values)
                                        */
#define MIN_INT -9223372036854775807   /**< Minimal value for int64 numbers
                                        *   (needed for YANG's special min and
                                        *   max values)
                                        */
#define JSON_MAX_STRING_LENGTH 2097152 /**< Minimal value of json strings
                                        *   (needed for YANG's special min and
                                        *   max values)
                                        */
#define YANG_VERSION 2                 /**< Value for "version" field in
                                        *   lys_module: conversion is based on
                                        *   YANG 1.1
                                        */
#define IGNORE_NODE 0x8000             /**< Flag to mark a node that is to be
                                        *   ignored
                                        */

using nlohmann::json_schema::json_validator;
using namespace std;
using json = nlohmann::json;
/**<
 * Uses nlohmann/json, call it json for convenience
 */

string outputDirString;
/**<
 * Global variable to hold the name of the output directory (used for both
 * conversion directions).
 */

map<string, sdfCommon*> typedefs;
/**<
 * Globally maps the names of YANG typedefs to their SDF equivalents.
 * This map is used together with the typerefs vector to assign sdfRef
 * references (conversion direction YANG->SDF).
 */
vector<tuple<string, string, sdfCommon*>> typerefs;
/**<
 * Global vector of tuples which each link a typedef name (with and without
 * prefix) to a pointer to an sdfCommon object that uses the typedef's SDF
 * equivalent.
 * Since the equivalent has possibly not been converted, this vector is used
 * as global storage to assign references (sdfRef) later together with
 * the typedefs vector (conversion direction YANG->SDF).
 */

map<string, sdfCommon*> identities;
/**<
 * Globally maps the names of YANG identities to their SDF equivalents.
 * This map is used together with the identsLeft vector to assign sdfRef
 * references (conversion direction YANG->SDF).
 */

vector<tuple<string, string, sdfCommon*>> identsLeft;
/**<
 * Global vector of tuples which each link an identity name (with and without
 * prefix) to a pointer to an sdfCommon object that uses the identitie's SDF
 * equivalent.
 * Since the equivalent has possibly not been converted, this vector is used
 * as global storage to assign references (sdfRef) later together with
 * the identities vector (conversion direction YANG->SDF).
 */

map<string, sdfCommon*> leafs;
/**<
 * Globally maps the names of YANG leafs to their SDF equivalents. This map is
 * used together with the referencesLeft vector to assign sdfRef references
 * (conversion direction YANG->SDF).
 */

vector<tuple<string, string, sdfCommon*>> referencesLeft;
/**<
 * Global vector of tuples which each link a leaf name (with and without prefix)
 * to a pointer to an sdfCommon object that uses the leaf's SDF equivalent in
 * a leafref. Since the equivalent has possibly not been converted, this vector
 * is used as global storage to assign references (sdfRef) later together with
 * the leafs vector (conversion direction YANG->SDF).
 */


map<string, sdfCommon*> branchRefs;
/**<
 * Globally maps the names of YANG branches to their SDF equivalents (conversion
 * direction YANG->SDF).
 */

vector<string> alreadyImported;
/**<
 * This vector is used to globally keep track of already converted and imported
 * modules by globally storing their names (conversion direction YANG->SDF).
 */

vector<shared_ptr<void>> voidPointerStore;
/**<
 * This vector is used as a global storage for smart void pointers
 * (conversion direction SDF->YANG).
 */

vector<string> stringStore;
/**<
 * This vector is used as a global storage for strings
 * (conversion direction SDF->YANG).
 */

vector<lys_revision> revStore;
/**<
 * This vector is used as a global storage for lys_revision objects
 * (conversion direction SDF->YANG).
 */

vector<shared_ptr<lys_node>> nodeStore;
/**<
 * This vector is used as a global storage for smart pointers to lys_nodes
 * (conversion direction SDF->YANG).
 */

vector<lys_tpdf*> tpdfStore;
/**<
 * This vector is used as a global storage for pointers to lys_tpdfs
 * (conversion direction SDF->YANG).
 */

vector<lys_restr> restrStore;
/**<
 * This vector is used as a global storage for lys_restrs
 * (conversion direction SDF->YANG).
 */

map<string, lys_ident*> identStore;
/**<
 * Globally maps sdfRef reference strings of sdfCommon objects to pointers to their YANG
 * lys_ident equivalents (conversion direction SDF->YANG).
 */

map<string, lys_node*> pathsToNodes;
/**<
 * Globally maps the references in sdfRefs to pointers to the lys_nodes equivalent to the
 * sdfRef's targets (conversion direction SDF->YANG).
 */

vector<tuple<string, lys_ident**>> openBaseIdent;
/**<
 * Global vector with tuples of sdfRef reference strings and their equivalent
 * referenced pointers to lys_ident pointers.
 * The vector is used only for round trips to assign the base identities
 * equivalent to the SDF elements (conversion direction SDF->YANG).
 */

vector<tuple<string, lys_node_augment*, string>> openAugments;
/**<
 * Global vector with tuples of name of the module of the open augmentation, pointer
 * to the lys_node_augment and the sdfRef reference to the target node's SDF
 * equivalent. The vector is used to store round trip'ed YANG augmentations
 * that have not been converted back (conversion direction SDF->YANG).
 */

vector<tuple<sdfFile*, lys_module*>> fileToModule;
/**<
 * Global vector to store tuples of pointers to sdfFiles and pointers to their
 * equivalent, converted lys_module (conversion direction SDF->YANG)
 */

lys_module *helper;
/**<
 * Global variable to hold the helper module that contains the sdf-spec
 * extension (conversion direction SDF->YANG).
 */

struct lys_tpdf stringTpdf = {
        .name = "string",
        .type = {.base = LY_TYPE_STRING}
};
/**<
 * Used to mark lys_types as type string (conversion direction SDF->YANG).
 */

struct lys_tpdf dec64Tpdf = {
        .name = "decimal64",
        .type = {.base = LY_TYPE_DEC64}
};
/**<
 * Used to mark lys_types as type decimal64 (conversion direction SDF->YANG).
 */

struct lys_tpdf intTpdf = {
        .name = "int64",
        .type = {.base = LY_TYPE_INT64}
};
/**<
 * Used to mark lys_types as type int64 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf uint64Tpdf = {
        .name = "uint64",
        .type = {.base = LY_TYPE_UINT64}
};
/**<
 * Used to mark lys_types as type uint64 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf int32Tpdf = {
        .name = "int32",
        .type = {.base = LY_TYPE_INT32}
};
/**<
 * Used to mark lys_types as type int32 (conversion direction SDF->YANG).
 */

struct lys_tpdf uint32Tpdf = {
        .name = "uint32",
        .type = {.base = LY_TYPE_UINT32}
};
/**<
 * Used to mark lys_types as type uint32 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf int16Tpdf = {
        .name = "int16",
        .type = {.base = LY_TYPE_INT16}
};
/**<
 * Used to mark lys_types as type int16 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf uint16Tpdf = {
        .name = "uint16",
        .type = {.base = LY_TYPE_UINT16}
};
/**<
 * Used to mark lys_types as type uint16 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf int8Tpdf = {
        .name = "int8",
        .type = {.base = LY_TYPE_INT8}
};
/**<
 * Used to mark lys_types as type int8 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf uint8Tpdf = {
        .name = "uint8",
        .type = {.base = LY_TYPE_UINT8}
};
/**<
 * Used to mark lys_types as type uint8 (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf booleanTpdf = {
        .name = "boolean",
        .type = {.base = LY_TYPE_BOOL}
};
/**<
 * Used to mark lys_types as type boolean (conversion direction SDF->YANG).
 */

struct lys_tpdf enumTpdf = {
        .name = "enumeration",
        .type = {.base = LY_TYPE_ENUM}
};
/**<
 * Used to mark lys_types as type enumeration (conversion direction SDF->YANG).
 */

struct lys_tpdf leafrefTpdf = {
        .name = "leafref",
        .type = {.base = LY_TYPE_LEAFREF}
};
/**<
 * Used to mark lys_types as type leafref (conversion direction SDF->YANG).
 */

struct lys_tpdf unionTpdf = {
        .name = "union",
        .type = {.base = LY_TYPE_UNION}
};
/**<
 * Used to mark lys_types as type union (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf emptyTpdf = {
        .name = "empty",
        .type = {.base = LY_TYPE_EMPTY}
};
/**<
 * Used to mark lys_types as type empty (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf bitsTpdf = {
        .name = "bits",
        .type = {.base = LY_TYPE_BITS}
};
/**<
 * Used to mark lys_types as type bits (only needed for round trips,
 * conversion direction SDF->YANG).
 */

struct lys_tpdf binaryTpdf = {
        .name = "binary",
        .type = {.base = LY_TYPE_BINARY}
};
/**<
 * Used to mark lys_types as type binary (conversion direction SDF->YANG).
 */

struct lys_tpdf identTpdf = {
        .name = "identityref",
        .type = {.base = LY_TYPE_IDENT}
};
/**<
 * Used to mark lys_types as type identityref (only needed for round trips,
 * conversion direction SDF->YANG).
 */


/**
 * Transform a const char array to string (NULL becomes the empty string)
 *
 * @param c The const char array to be transformed into a string
 * @return The input as a string
 */
string avoidNull(const char *c);

/**
 * Store a node in a the global storage variable for global access
 *
 * @param node A smart pointer to the node to be stored
 * @return A pointer to the stored node
 */
lys_node* storeNode(shared_ptr<lys_node> node);

/**
 * Store a string in a the global storage variable for global access
 *
 * @param str The string to be stored
 * @return A pointer to the stored string as a C-string
 */
const char* storeString(string str);

/**
 * Store a lys_restr in a the global storage variable for global access
 *
 * @param restr The lys_restr to be stored
 * @return A pointer to the stored lys_restr
 */
lys_restr* storeRestriction(lys_restr restr);

/**
 * Store a pointer to a lys_tpdf in a the global storage variable for global
 * access.
 *
 * @param tpdf The pointer to a lys_tpdf to be stored
 * @return The pointer
 */
lys_tpdf* storeTypedef(lys_tpdf *tpdf);

/**
 * Store a void pointer in a the global storage variable for global access
 *
 * @param v A smart void pointer to be stored
 * @return The stored void pointer
 */
void* storeVoidPointer(shared_ptr<void> v);


sdfData* nodeToSdfData(struct lys_node *node, sdfObject *object);

/**
 * Remove all quotation marks from a given string
 *
 * @param input The string in question
 * @return The input string without quotation marks
 */
string removeQuotationMarksFromString(string input);

/**
 * Determine whether a given lys_type uses a given lys_module
 *
 * @param type A pointer to the lys_type in question
 * @param wanted A pointer to the lys_module in question
 * @return True if the type uses the module, false otherwise
 */
bool typeUsesModule(lys_type *type, lys_module *wanted);

/**
 * Determine whether a given array of lys_tpdfs uses a given lys_module
 *
 * @param tpdfs An array with the lys_tpdfs in question
 * @param tpdfs_size The size of the lys_tpdfs array
 * @param wanted A pointer to the lys_module in question
 * @return True if any of the lys_tpdfs in the array uses the module, false
 * otherwise
 */
bool tpdfsUseModule(lys_tpdf *tpdfs, uint16_t tpdfs_size, lys_module *wanted);

/**
 * Determine whether a given array of lys_idents uses a given lys_module
 *
 * @param idents An array with the lys_idents in question
 * @param ident_size The size of the lys_idents array
 * @param wanted A pointer to the lys_module in question
 * @return True if any of the lys_idents in the array uses the module, false
 * otherwise
 */
bool identsUseModule(lys_ident *idents, uint16_t ident_size, 
        lys_module *wanted);

/**
 * Determine whether a given lys_node uses a given lys_module
 *
 * @param node A pointer to the lys_node in question
 * @param wanted A pointer to the lys_module in question
 * @return True if the lys_node uses the module, false otherwise
 */
bool subTreeUsesModule(lys_node *node, lys_module *wanted);

/**
 * Determine whether a given lys_module uses another given lys_module
 *
 * @param module A pointer to the first lys_module in question
 * @param wanted A pointer to the lys_module that is wanted
 * @return True if the first lys_module uses the other module, false otherwise
 */
bool moduleUsesModule(lys_module *module, lys_module *wanted);

/**
 * Recursively generate the path of a lys_node
 * 
 * If the given lys_module differs from the module of the node, or if stated 
 * through the addPrefix parameter, add a prefix
 * 
 * @param node A pointer to the lys_node
 * @param module A pointer to the lys_module
 * @param addPrefix Whether or not to add a prefix
 * 
 * @return The path of the given lys_node
 */
string generatePath(lys_node *node, lys_module *module = NULL,
        bool addPrefix = false);

/**
 * Expand the target path on a lys_node_leaf with type leafref
 * 
 * For a given leaf node that has the type leafref expand the target path
 * given by the leafref so that it does not contain ".." anymore
 * but specifies the full path. It is not possible to simply call generatePath 
 * on the target in the node info because the target node is not always given.
 * 
 * @param node A pointer to the lys_node_leaf with type leafref
 * @param addPrefix Whether or not to add a prefix
 * 
 * @return The expanded path of the to the leafref target node
 */
string expandPath(lys_node_leaf *node, bool addPrefix = false);

/**
 * Expand the target path on a lys_node_leaflist with type leafref
 * 
 * Overloaded function: For a given leaf-list node that has the type leafref expand the target path
 * given by the leafref so that it does not contain ".." anymore
 * but specifies the full path. It is not possible to simply call generatePath 
 * on the target in the node info because the target node is not always given.
 * 
 * @param node A pointer to the lys_node_leaflist with type leafref
 * @param addPrefix Whether or not to add a prefix
 * 
 * @return The expanded path of the to the leafref target node
 */
string expandPath(lys_node_leaflist *node, bool addPrefix = false);

/**
 * Expand the target path on a lys_tpdf with type leafref
 * 
 * Overloaded function: For a given typedef that has the type leafref expand the target path
 * given by the leafref so that it does not contain ".." anymore
 * but specifies the full path. It is not possible to simply call generatePath 
 * on the target in the node info because the target node is not always given.
 * 
 * @param tpdf A pointer to the lys_tpdf with type leafref
 * @param addPrefix Whether or not to add a prefix
 * 
 * @return The expanded path of the to the leafref target node
 */
string expandPath(lys_tpdf *tpdf, bool addPrefix = false);


/**
 * Recursively check all parent nodes of a lys_node for given type
 * 
 * @param _node The lys_node that is the starting point of the search
 * @param type the LYS_NODE type to check for
 * 
 * @return Whether or not any ancestor node of the given node has the given type
 */
bool someParentNodeHasType(struct lys_node *_node, LYS_NODE type);

/**
 * Deduce from a given libyang base type the corresponding JSON type.
 * 
 * @param type The libyang base type
 * 
 * @return The deduced JSON type (represented as defined by SDF de-/serialiser)
 */
jsonDataType parseBaseType(LY_DATA_TYPE type);

/**
 * Deduce the libyang base type from a given string.
 * 
 * @param type The string
 * 
 * @return The deduced libyang base type
 */
LY_DATA_TYPE stringToLType(std::string type);

/**
 * Deduce from a given JSON type the corresponding libyang base type.
 * 
 * @param type The JSON type (represented as defined by SDF de-/serialiser)
 * 
 * @return The deduced libyang base type
 */
LY_DATA_TYPE stringToLType(jsonDataType type);

/**
 * Extract the type from a given lys_type as string
 * 
 * @param type A pointer to the given lys_type
 * 
 * @return The type in string form
 */
string parseTypeToString(struct lys_type *type);

/**
 * Add a conversion note to a given sdfCommon
 * 
 * @param com A pointer to the sdfCommon to add the origin note to
 * @param stmt The statement part of the conversion note
 * @param arg The argument part of the conversion note
 */
void addOriginNote(sdfCommon *com, string stmt, string arg = "");

/**
 * Transform a range in const char array form into an integer vector
 * 
 * @param range A const char array with the range
 * 
 * @return An std::vector<int64_t> containing the single range values
 */
vector<int64_t> rangeToInt(const char *range);


/**
 * Uses a regular expression to take apart range C-strings (e.g. "0..1" to 0 and 1) into a float vector
 * 
 * @param range A const char array with the range
 * 
 * @return An std::vector<float> containing the single range values
 */
vector<float> rangeToFloat(const char *range);

/**
 * Put two floats into a single const char array as used by libyang
 * 
 * @param min The minimum in the range
 * @param max The maximum in the range
 * 
 * @return An const char array with the range as used by libyang
 */
const char * floatToRange(float min, float max);

/**
 * Put two integers into a single const char array as used by libyang
 * 
 * @param min The minimum in the range
 * @param max The maximum in the range
 * 
 * @return An const char array with the range as used by libyang
 */
const char * intToRange(int64_t min, uint64_t max);

/**
 * Put the status flags of libyang into a conversion note in a given description string
 * 
 * @param flags The flags from libyang
 * @param dsc The original description string
 * 
 * @return The altered description string
 */
string statusToDescription(uint16_t flags, string dsc);

/**
 * Put the must restriction of libyang into a conversion note in a given description string
 * 
 * @param must An array of lys_restr containing the must constraint
 * @param size The size of the must array
 * @param dsc  The original description string
 * 
 * @return The altered description string
 */
string mustToDescription(lys_restr *must, int size, string dsc);

/**
 * Put the when restriction of libyang into a conversion note in a given description string
 * 
 * @param when A pointer to the lys_restr containing the when constraint
 * @param dsc The original description string
 * 
 * @return The altered description string
 */
string whenToDescription(lys_when *when, string dsc);

/**
 *  Information from the given lys_type struct is translated
 *  and set accordingly in the given sdfData element
 *  
 *  @param type A pointer to the lys_type in question
 *  @param data A pointer to the sdfData in question
 *  @param parentIsTpdf Whether or not the parent of the lys_type is a lys_tpdf
 *  
 *  @return A pointer to the altered sdfData
 */
sdfData* typeToSdfData(struct lys_type *type, sdfData *data,
        bool parentIsTpdf = false);

/**
 * The information is extracted from the given lys_tpdf struct and
 * a corresponding sdfData object is generated
 * 
 * @param tpdf A pointer to the lys_tpdf in to be converted
 * 
 * @return A pointer to the generated sdfData object
 */
sdfData* typedefToSdfData(struct lys_tpdf *tpdf);

/**
 * The information is extracted from the given lys_node_leaf struct and
 * a corresponding sdfProperty object is generated
 * 
 * @param node A pointer to the lys_node_leaf in question
 * @param object A pointer to the sdfObject the sdfProperty belongs to (if any)
 * 
 * @return A pointer to the generated sdfProperty object
 */
sdfProperty* leafToSdfProperty(struct lys_node_leaf *node,
        sdfObject *object = NULL);

/**
 * The information is extracted from the given lys_node_leaf struct and
 * a corresponding sdfData object is generated
 * 
 * @param node A pointer to the lys_node_leaf in question
 * @param object A pointer to the sdfObject the sdfData belongs to (if any)
 * 
 * @return A pointer to the generated sdfData object
 */
sdfData* leafToSdfData(struct lys_node_leaf *node,
        sdfObject *object = NULL);

/**
 * The information is extracted from the given lys_node_leaflist struct and
 * a corresponding sdfProperty object is generated
 * 
 * @param node A pointer to the lys_node_leaflist in question
 * @param object A pointer to the sdfObject the sdfProperty belongs to (if any)
 * 
 * @return A pointer to the generated sdfProperty object
 */
sdfProperty* leaflistToSdfProperty(struct lys_node_leaflist *node,
        sdfObject *object = NULL);

/**
 * The information is extracted from the given lys_node_leaflist struct and
 * a corresponding sdfData object is generated
 * 
 * @param node A pointer to the lys_node_leaflist in question
 * @param object A pointer to the sdfObject the sdfData belongs to (if any)
 * 
 * @return A pointer to the generated sdfData object
 */
sdfData* leaflistToSdfData(struct lys_node_leaflist *node,
        sdfObject *object = NULL);

/**
 * The information is extracted from the given lys_node_container struct and
 * a corresponding sdfThing object is generated
 * 
 * @param node A pointer to the lys_node_container in question
 * 
 * @return A pointer to the generated sdfThing object
 */
sdfThing* containerToSdfThing(struct lys_node_container *node);

/**
 * The information is extracted from the given lys_node_rpc_action struct and
 * a corresponding sdfAction object is generated
 * 
 * In case of an action node, data has to be given
 * 
 * @param node A pointer to the lys_node_rpc_action in question
 * @param object A pointer to the sdfObject the sdfAction belongs to (if any)
 * @param data In case of an action type node, data has to be given. SdfData pointer correspondig to the container an action belongs to
 */
sdfAction* actionToSdfAction(lys_node_rpc_action *node, sdfObject *object,
        sdfData *data = NULL);

/**
 * Read information from the sdf-spec YANG extension (if ext is this extension)
 * 
 * @param ext A pointer to the lys_ext_instance array in question
 * @param ext_size number of element in ext
 * 
 * @return A string with the argument of the sdf-spec extension (if ext is this extension)
 */
string sdfSpecExtToString(lys_ext_instance **ext, int ext_size);

/**
 * This function is borrowed and modified from resolve.c and resolves the
 * operator used in lys_iffeature structs.
 * 
 * @param list The exression array that contains the coded operator
 * @param pos  The position at which the operator can be found in list
 * 
 * @return The operator
 */
uint8_t iff_getop(uint8_t *list, int pos);

/**
 * This function is borrowed and modified from resolve.c of libyang and resolves
 * the lys_iffeature back into a string recursively.
 * 
 * @param expr    The lys_iffeature struct to resolve
 * @param index_e The current position in expr
 * @param index_f The current position in the features array of expr
 * 
 * @return An if-feature string as it would be printed in a YANG module
 */
string resolve_iffeature_recursive(struct lys_iffeature *expr, int *index_e, 
        int *index_f);

/**
 * Convert the content of a given lys_node into an sdfData object
 * 
 * @param node   A pointer to the lys_node to be converted
 * @param object A pointer to the sdfObject the sdfData element belongs to (if 
 *               any)
 * 
 * @return A pointer to an sdfData object that corresponds to the given node
 */
sdfData* nodeToSdfData(struct lys_node *node, sdfObject *object);

/**
 * Convert a lys_ident struct into an sdfData object
 * 
 * @param _ident The lys_ident struct to be converted
 * 
 * @return A pointer to an sdfData object that corresponds to the given 
 *         lys_ident
 */
sdfData* identToSdfData(struct lys_ident _ident);

/**
 * Looks in a map of reference strings and corresponding sdfCommon objects to assign the given open references if possible
 * 
 * @param refsLeft A vector with tuples of reference strings and the sdfCommon
 *                 that has the open reference
 * @param refs     A map of references to the sdfCommon objects they reference
 * 
 * @return A vector with references that are still not assinged 
 */
vector<tuple<string, string, sdfCommon*>> assignReferences(
        vector<tuple<string, string, sdfCommon*>> refsLeft,
        map<string, sdfCommon*> refs);

sdfFile* moduleToSdfFile(lys_module *module);

/**
 * The information is extracted from the given lys_module struct and
 * a corresponding sdfOjbect object is generated or filled (deprecated)
 * 
 * @param module A pointer to the given lys_module struct
 * @param object A pointer to an already existing sdfObject to be filled
 * 
 * @return A pointer to the generated or filled sdfObject corresponding to the 
 *         given module
 */
sdfObject* moduleToSdfObject(const struct lys_module *module,
        sdfObject *object =  NULL);

/**
 * The information is extracted from the given lys_node_container struct and
 * a corresponding sdfOjbect object is generated or filled
 * 
 * @param cont   A pointer to the given lys_node_container struct
 * @param object A pointer to an already existing sdfObject to be filled
 * 
 * @return A pointer to the generated or filled sdfObject corresponding to the 
 *         given container node
 */
sdfObject* containerToSdfObject(lys_node_container *cont, 
        sdfObject *object = NULL);

void removeNode(lys_node &node);
void addNode(lys_node &child, lys_node &parent, lys_module &module);

/**
 * The information is extracted from the given lys_node_container struct and
 * a corresponding sdfThing object is generated or filled
 * 
 * @param cont   A pointer to the given lys_node_container struct
 * @param thing A pointer to an already existing sdfThing to be filled
 * 
 * @return A pointer to the generated or filled sdfThing corresponding to the 
 *         given container node
 */
sdfThing* containerToSdfThing(lys_node_container *cont, sdfThing *thing = NULL);

/**
 * The information from a given lys_module struct is converted into an sdfFile
 * 
 * @param module A pointer to the lys_module to be converted
 * 
 * @return A pointer to the corresponding converted sdfFile
 */
sdfFile* moduleToSdfFile(lys_module *module);

/**
 * Add an sdf-spec extension instance with a specified argument to the given 
 * list of extension instances
 * 
 * @param arg       The string to add as an argument to the new sdf-spec
 *                  extension instance
 * @param exts      An array of pointers to the existing extension instances
 * @param exts_size The number of elements in the exts array
 * 
 * @return A pointer to an lys_ext_instance array containing the new
 *         extension (as used by libyang)
 */
lys_ext_instance** argToSdfSpecExtension(string arg, lys_ext_instance **exts,
        uint8_t exts_size);

/**
 * Add the sdf-spec extension to a lys_type
 * 
 * @param tpdf A pointer to the lys_type in question
 * @param arg  The string to add as an argument of the sdf-spec extension
 */
void setSdfSpecExtension(lys_type *type, string arg);

/**
 * Add the sdf-spec extension to a lys_tpdf
 * 
 * @param tpdf A pointer to the lys_tpdf in question
 * @param arg  The string to add as an argument of the sdf-spec extension
 */
void setSdfSpecExtension(lys_tpdf *tpdf, string arg);

/**
 * Add the sdf-spec extension to a lys_node
 * 
 * @param node A pointer to the lys_node in question
 * @param arg  The string to add as an argument of the sdf-spec extension
 */
void setSdfSpecExtension(lys_node *node, string arg);

/**
 * Erase all conversion notes from a string
 * 
 * @param dsc The original string
 *
 * @return The altered string
 */
string removeConvNote(string dsc);

/**
 * Erase all conversion notes from a char array (C-string) and convert it to string
 * 
 * @param dsc The original C-string
 *
 * @return The altered string
 */
string removeConvNote(char *dsc);

/**
 * Extract all conversion notes from the description of a given sdfCommon object
 * 
 * @param com A pointer to the sdfCommon object in question
 * 
 * @return A vector containing tuples of conversion note statements and 
 *         arguments
 */
vector<tuple<string, string>> extractConvNote(sdfCommon *com);

/**
 * Set a given lys_node to mandatory according to the rules from RFC 7950
 * 
 * RFC 7950 3. Terminology: A mandatory node is
 * a leaf/choice/anydata/anyxml node with "mandatory true",
 * a list/leaf-list node with "min-elements" > 0
 * or a non-presence container node with at least one mandatory child node
 * 
 * @param node       A pointer to the lys_node in question
 * @param firstLevel Whether to only check the first level of child nodes
 * 
 * @return Whether or not the given lys_node could be made mandatory
 */
bool setMandatory(lys_node *node, bool firstLevel = true);

/**
 * Convert the sdfRequired quality to YANG
 * 
 * @param reqs   A vector with pointers to the sdfCommon objects in the
 *               sdfRequired list
 * @param module The address of the module in question
 */
void sdfRequiredToNode(vector<sdfCommon*> reqs, lys_module &module);

/**
 * Fill the information of an sdfData element into lys_type
 * 
 * @param data         A pointer to the sdfData element to be converted
 * @param type         The address of a lys_type to be filled
 * @param openRefsType The address of the vector with open type references
 *                     to add to
 */
void fillLysType(sdfData *data, struct lys_type &type,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Add a specified lys_node to the given parent lys_node
 * 
 * @param child  The address of the child lys_node to be added
 * @param parent The address of the parent lys_node to add the child lys_node to
 * @param module The lys_module the parent lys_node belongs to 
 */
void addNode(lys_node &child, lys_node &parent, lys_module &module);

/**
 * Add a specified lys_node to the given lys_module on top-level
 * 
 * @param child  The address of the child lys_node to be added
 * @param module The address of the lys_module to add the child node to
 */
void addNode(lys_node &child, lys_module &module);

/**
 * Remove a given lys_node from its tree
 * 
 * @param node The address of the lys_node to be removed
 */
void removeNode(lys_node &node);

/**
 * Find the derived type that corresponds to a given reference string
 * 
 * @param refString The string containing the reference
 * @param type A pointer to the type that needs to be assigned
 * 
 * @return A pointer to the assigned type
 */
lys_type* sdfRefToType(string refString, lys_type *type);

lys_module* sdfFileToModule(sdfFile &file, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Traverse the subtree of a node and return the first found leaf node
 * 
 * @param node The node whose subree should be traversed and searched
 * @return A pointer to the leaf node that was found, NULL if none was found
 */
lys_node_leaf* findLeafInSubtreeRecursive(lys_node *node);

/**
 * Find what an sdfRef corresponds to in YANG and assign that to the specified
 * lys_node (which an also by a lys_tpdf)
 * 
 * @param com        A pointer to the sdfCommon object that has the sdfRef
 * @param node       A pointer to the lys_node with the open reference
 * @param module     The address of the module of the lys_node
 * @param nodeIsTpdf Whether or not the node is actually a lys_tpdf
 * 
 * @return The lys_node that has now been assigned the reference, NULL if 
 *         nothing was found
 */
lys_node* sdfRefToNode(sdfCommon *com, lys_node *node, lys_module &module,
        bool nodeIsTpdf = false);

/**
 * Convert the information from an sdfData element into a lys_tpdf
 * 
 * @param data         A pointer to the sdfData element to be converted
 * @param tpdf         A pointer to the target lys_tpdf
 * @param module       The address of the module tpdf belongs to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A pointer to the converted lys_tpdf corresponding to the given 
 *         sdfData element
 */
struct lys_tpdf* sdfDataToTypedef(sdfData *data, lys_tpdf *tpdf,
        lys_module &module, vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert the information from an sdfData element into a lys_node
 * 
 * @param data         A pointer to the sdfData element to be converted
 * @param node         A pointer to the target lys_node
 * @param module       The address of the module node belongs to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A pointer to the converted lys_tpdf corresponding to the given 
 *         sdfData element
 */
lys_node* sdfDataToNode(sdfData *data, lys_node *node, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert the given data types (vector of sdfData elements) and add them to a 
 * given lys_module
 * 
 * @param datatypes    The vector with the data types (sdfData element pointers)
 * @param module       The address of the lys_module in question
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 */
void convertDatatypes(vector<sdfData*> datatypes, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert the information block of an SDF model to add to a YANG module
 * 
 * @param info   A pointer to the given sdfInfoBlock object
 * @param module The address of the module to add the information to
 */
void convertInfoBlock(sdfInfoBlock *info, lys_module &module);

/**
 * Convert the namespace section of an SDF model to add to a YANG module
 * 
 * @param ns           A pointer to the given sdfNamespaceSection object
 * @param module       The address of the module to add the information to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 */
void convertNamespaceSection(sdfNamespaceSection *ns, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Assign open references
 * 
 * @param module       The address of the lys_module in question
 * @param openRefs     The address of the vector of open node references to
 *                     assign
 * @param openRefsTpdf The address of the vector of open typedef references to
 *                     assign
 * @param openRefsType The address of the vector of open type references to 
 *                     assing
 */
void assignOpenRefs(lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Import the helper module that contains the sdf-spec extension to the given 
 * module
 * 
 * @param module The address of the importing module
 */
void importHelper(lys_module &module);

/**
 * Convert a number of sdfProperties and add them to a YANG module
 * 
 * @param props        A pointer to the vector of sdfProperties to be converted
 * @param module       The address of the lys_module to add the converted 
 *                     sdfProperties to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsType The address of the vector to add open type references to
 */
void convertProperties(vector<sdfProperty*> props, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert a number of sdfActions and add them to a YANG module
 * 
 * @param actions      A pointer to the vector of sdfActions to be converted
 * @param module       The address of the lys_module to add the converted 
 *                     sdfActions to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 */
void convertActions(vector<sdfAction*> actions, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert a number of sdfEvents and add them to a YANG module
 * 
 * @param events       A pointer to the vector of sdfEvents to be converted
 * @param module       The address of the lys_module to add the converted 
 *                     sdfEvents to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 */
void convertEvents(vector<sdfEvent*> events, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);


struct lys_module* sdfThingToModule(sdfThing &thing, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert a number of sdfThings and add them to a YANG module
 * 
 * @param things       A pointer to the vector of sdfThings to be converted
 * @param module       The address of the lys_module to add the converted 
 *                     sdfThings to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A vector of lys_node pointers populated with the information
 */
vector<lys_node*> convertThings(vector<sdfThing*> things, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert an sdfObject and add the converted equivalent to a given lys_module
 * 
 * @param object       The address of the sdfObject to be converted
 * @param module       The address of the lys_module to add the converted
 *                     sdfObject to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A pointer to the lys_module struct that has been populated with the 
 *         information
 */
struct lys_module* sdfObjectToModule(sdfObject &object, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Convert a number of sdfObjects and add them to a given lys_module
 * 
 * @param objects      A pointer to the vector of sdfObjects to be converted
 * @param module       The address of the lys_module to add the converted 
 *                     sdfObjects to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A vector of lys_node pointers populated with the information
 */
vector<lys_node*> convertObjects(vector<sdfObject*> objects, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Information from an sdfThing is transferred into a given lys_module struct
 * 
 * @param thing        The address of the sdfThing to be converted
 * @param module       The address of the lys_module to add the converted
 *                     sdfThing to
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A pointer to the lys_module struct that has been populated with the 
 *         information
 */
struct lys_module* sdfThingToModule(sdfThing &thing, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * Information from an sdfFile object is transferred into a given 
 * lys_module struct
 * 
 * @param file         The address of the sdfFile object to be converted
 * @param module       The address of the lys_module equivalent to the converted
 *                     sdfFile
 * @param openRefs     The address of the vector to add open node references to
 * @param openRefsTpdf The address of the vector to add open typedef references 
 *                     to
 * @param openRefsType The address of the vector to add open type references to
 * 
 * @return A pointer to the lys_module struct that has been populated with the 
 *         information
 */
struct lys_module* sdfFileToModule(sdfFile &file, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

/**
 * The main function that is executed on execution of the tool
 * 
 * @param argc Number of elements in argv
 * @param argv Input to the tool (array of C-strings)
 * 
 * @return 0 on successful execution, -1 else
 */
int main(int argc, const char** argv);

#endif
