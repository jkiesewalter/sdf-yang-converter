#include <iostream>
#include <fstream>
#include<sstream>
#include<stdio.h>
#include<string>
#include <regex>
#include <libyang/libyang.h>
#include <nlohmann/json.hpp>
#include "sdf.hpp"

// for convenience
using json = nlohmann::json;

map<string, sdfCommon*> typedefs;
map<string, sdfCommon*> identities;

string avoidNull(const char *c)
{
	if (c == NULL)
		return "";
	else
		return string(c);
}

jsonDataType parseBaseType(LY_DATA_TYPE type)
{
	if (type == LY_TYPE_BOOL)
		return json_boolean;
	else if (type == LY_TYPE_DEC64)
		return json_number;
	else if (type == LY_TYPE_INT8
			|| type == LY_TYPE_UINT8
			|| type == LY_TYPE_INT16
			|| type == LY_TYPE_UINT16
			|| type == LY_TYPE_INT32
			|| type == LY_TYPE_UINT32
			|| type == LY_TYPE_INT64
			|| type == LY_TYPE_UINT64)
		return json_integer;
	// TODO: will all enums be translated to type string?
	else if (type == LY_TYPE_STRING || type == LY_TYPE_ENUM)
		return json_string;
	else
	{
		// error handling?
		//cerr << "The base type " << type << " could not be resolved" << endl;
		return json_type_undef;
	}

    // TODO: what about LY_TYPE_BINARY, LY_TYPE_BITS, LY_TYPE_EMPTY,
    // LY_TYPE_IDENT, LY_TYPE_INST, LY_TYPE_LEAFREF, LY_TYPE_UNKNOWN?
}
/*
vector<jsonDataType> parseType(struct lys_type *type)
//jsonDataType parseType(struct lys_type *type)
{
    if (type->base == LY_TYPE_UNION)
    {
    	vector<jsonDataType> result;
    	for (int i = 0; i < type->info.uni.count; i++)
    		result.push_back(parseBaseType(type->info.uni.types[i].base));
    	return result;
    }
    else
    	return {parseBaseType(type->base)};
}
*/

string parseTypeToString(struct lys_type *type)
{
	if (type->base == LY_TYPE_BOOL)
		return "boolean";
	else if (type->base == LY_TYPE_DEC64)
		return "number";
	else if (type->base == LY_TYPE_INT8
			|| type->base == LY_TYPE_UINT8
			|| type->base == LY_TYPE_INT16
			|| type->base == LY_TYPE_UINT16
			|| type->base == LY_TYPE_INT32
			|| type->base == LY_TYPE_UINT32
			|| type->base == LY_TYPE_INT64
			|| type->base == LY_TYPE_UINT64)
		return "integer";
	// TODO: will all enums be translated to type string?
	else if (type->base == LY_TYPE_STRING || type->base == LY_TYPE_ENUM)
		return "string";
	else
	{
		// error handling?
		//cerr << "The type " << type->der->name << " could not be resolved" << endl;
		return "";
	}

    // TODO: what about LY_TYPE_BINARY, LY_TYPE_BITS, LY_TYPE_EMPTY,
    // LY_TYPE_IDENT, LY_TYPE_INST, LY_TYPE_LEAFREF, LY_TYPE_UNKNOWN?
}

sdfData* parseDefault(const char *value, sdfData *data)
{
	if (value != NULL)
	{
		if (data->getSimpType() == json_string)
			data->setDefaultString(value);

		else if (value != "")
		{
			if(data->getSimpType() == json_number)
				data->setDefaultNumber(atof(value));

			else if(data->getSimpType() == json_boolean)
			{
				cout << typeid(value).name() << endl;
				if (value == "true")
					data->setDefaultBool(true);
				else if (value == "false")
					data->setDefaultBool(false);
			}
			else if(data->getSimpType() == json_integer)
				data->setDefaultInt(atoi(value));
		}
	}
	return data;
}

// uses regex to take apart range strings (e.g. "0..1" to 0 and 1)
vector<float> rangeToFloat(const char *range)
{
    cmatch cm;
    regex min_max_regex("(.*)\\.\\.(.*)");
    regex_match(range, cm, min_max_regex);
    return {stof(cm[1].str()), stof(cm[2].str())};

}

// the element is translated according to its type
sdfData* typeToSdfData(struct lys_type *type, sdfData *data)
{
	// if type does not refer to a built-in type
	if (typedefs[type->der->name] != NULL && data->getSimpType() != json_array)
	{
		data->setType(type->der->name);
		data->setReference(typedefs[type->der->name]);
		return data;
	}

	if (type->base == LY_TYPE_IDENT)
	{
		// TODO: only one sdfRef possible, what if there are multiple bases?
		data->setDerType("");
		data->setReference(identities[type->info.ident.ref[0]->name]);
	}

	// TODO:
	// as of now, enums are always converted to string type
	else if (type->base == LY_TYPE_ENUM)
		data->setSimpType(json_string);

	jsonDataType jsonType = data->getSimpType();
	// check for types and translate accordingly
	// number
	if (jsonType == json_number)
	{
		// float cnst = NAN;
		float min = NAN;
		float max = NAN;
		float mult = type->info.dec64.div;
		vector<float> enm = {};
		if (type->info.dec64.range != NULL)
		{
			vector<float> min_max
				= rangeToFloat(type->info.dec64.range[0].expr);
			min = min_max[0];
			max = min_max[1];
		}
		data->setNumberData(NAN, NAN, min, max, mult, enm);
	}

	// string
	else if (jsonType == json_string)
	{
		string cnst = "";
		float minLength = NAN;
		float maxLength = NAN;
		jsonSchemaFormat format = json_format_undef;
		string pattern = "";
		vector<string> enum_names = {};

		// TODO: first byte of expr specifies whether to match or invert-match
		// TODO: are the regexs of SDF and YANG compatible?
		if (type->info.str.pat_count > 0)
		{
			pattern = type->info.str.patterns[0].expr;
			// TODO: what about more than one pattern in patterns?
		}

		// TODO: can enums from yang really only be translated to string type?
		if (type->base == LY_TYPE_ENUM)
		{
			for (int i = 0; i < type->info.enums.count; i++)
			{
				enum_names.push_back(type->info.enums.enm[i].name);
				data->setDescription(data->getDescription() + "\n"
						+ type->info.enums.enm[i].name + ": "
						+ type->info.enums.enm[i].dsc);
			}
		}
		else if (type->info.str.length != NULL)
		{
			//minLength = atoi(tpdf->type.info.str.length[0].expr);
			//maxLength = atoi(tpdf->type.info.str.length[1].expr);
			vector<float> min_max
				= rangeToFloat(type->info.str.length[0].expr);
			minLength = min_max[0];
			maxLength = min_max[1];
		}

		data->setStringData("", "", minLength, maxLength,
				pattern, format, enum_names);
	}

	// boolean
	else if (jsonType == json_boolean)
	{
		vector<bool> enm = {};
		data->setBoolData(false, false, false, false, enm);
	}

	// integer
	else if (jsonType == json_integer)
	{
		//int cnst = -1;
		//bool cnstdef = false;
		float min = NAN;
		float max = NAN;
		vector<int> enm = {};
		if (type->info.num.range != NULL)
		{
			vector<float> min_max
				= rangeToFloat(type->info.num.range[0].expr);
			min = min_max[0];
			max = min_max[1];
		}
		data->setIntData(-1, false, -1, false, min, max, enm);
	}

	else if (jsonType == json_array)
	{
		// TODO: default value (template?)
		float minItems = NAN;
		float maxItems = NAN;
		bool uniqeItems = false;
		string item_type;
		sdfCommon *ref = NULL;
		//if (type->base == LY_TYPE_DER)
		if (typedefs[type->der->name] != NULL)
		{
			item_type = type->der->name;
			ref = typedefs[type->der->name];
		}
		else
			item_type = parseTypeToString(type);
		data->setArrayData(vector<string>(), minItems, maxItems, uniqeItems,
				type->der->name, ref, NAN, NAN);
	}

	return data;
}

sdfData* typedefToSdfData(struct lys_tpdf *tpdf)
{
	//vector<jsonDataType> type = parseType(&tpdf->type);
	sdfData *data = new sdfData(avoidNull(tpdf->name), avoidNull(tpdf->dsc),
			parseTypeToString(&tpdf->type));

	// translate units
	string units = "";
	if (tpdf->units != NULL)
	{
		data->setUnits(tpdf->units);
	}

	typeToSdfData(&tpdf->type, data);
	parseDefault(tpdf->dflt, data);

	typedefs[tpdf->name] = data;
	return data;
}

sdfProperty* leafToSdfProperty(struct lys_node_leaf *node)
{
	sdfProperty *property = new sdfProperty(avoidNull(node->name), avoidNull(node->dsc),
			parseBaseType(node->type.base));
	typeToSdfData(&node->type, property);
	if (node->units != NULL)
	{
		property->setUnits(node->units);
	}
	parseDefault(node->dflt, property);

	// TODO: keyword mandatory -> sdfRequired

	return property;
}

sdfProperty* leaflistToSdfProperty(struct lys_node_leaflist *node)
{
	sdfProperty *property = new sdfProperty(avoidNull(node->name),
			avoidNull(node->dsc), json_array);
	typeToSdfData(&node->type, property);
	// the number of minimal and maximal items is only valid if at least
	// one of them is not 0
	if (node->min != 0 || node->max != 0)
	{
		property->setMinItems(node->min);
		property->setMaxItems(node->max);
	}
	// translate units
	if (node->units != NULL)
	{
		property->setUnits(node->units);
	}

	// TODO: default values for other types (template?)
	vector<string> dflt;
	for (int i = 0; i < node->dflt_size; i++)
		dflt.push_back(node->dflt[i]);
	property->setDefaultArray(dflt);
	// type array
    // items of type sdfData-typedef of leaflist for complex datatypes?
	//sdfData *datatype = new sdfData();

	return property;
}

sdfAction* rpcToSdfAction(struct lys_node_rpc_action *node)
{
	sdfAction *action = new sdfAction(avoidNull(node->name), avoidNull(node->dsc));

	// convert typedefs into sdfData of the action
	for (int i = 0; i < node->tpdf_size; i++)
		action->addDatatype(typedefToSdfData(&node->tpdf[i]));

	return action;
}

sdfEvent* notificationToSdfEvent(struct lys_node_notif *node)
{
	sdfEvent *event = new sdfEvent(avoidNull(node->name), avoidNull(node->dsc));

	return event;
}

sdfThing* containerToSdfThing(struct lys_node_container *node)
{
	sdfThing *thing = new sdfThing(avoidNull(node->name), avoidNull(node->dsc));
	return thing;
}

sdfThing* groupingToSdfThing(struct lys_node_grp *node)
{
	sdfThing *thing = new sdfThing(avoidNull(node->name), avoidNull(node->dsc));

	return thing;
}

sdfThing* submoduleToSdfThing(struct lys_submodule *submod)
{
	sdfThing *thing = new sdfThing(avoidNull(submod->name), avoidNull(submod->dsc));

	return thing;
}

sdfThing* moduleToSdfThing(const struct lys_module *module)
{

	sdfThing *thing = new sdfThing(avoidNull(module->name),
			avoidNull(module->dsc));
    thing->setInfo(sdfInfoBlock(avoidNull(module->name)));
    thing->setNamespace(sdfNamespaceSection(avoidNull(module->ns),
    		avoidNull(module->prefix)));

    if (module->tpdf_size > 0)
    {
    	sdfObject *tpdfObject = new sdfObject("typedefs",
    			"The derived types (typedefs) used in " + thing->getLabel());
    	thing->addObject(tpdfObject);
    	for (int i = 0; i < module->tpdf_size; i++)
		{
    		tpdfObject->addDatatype(typedefToSdfData(&module->tpdf[i]));
		}
    }
    if (module->ident_size > 0)
	{
		sdfObject *tpdfObject = new sdfObject("identities",
				"The identities used in " + thing->getLabel());
		sdfData *ident;
		thing->addObject(tpdfObject);
		for (int i = 0; i < module->ident_size; i++)
		{
			ident = new sdfData(module->ident[i].name, module->ident[i].dsc, "");
			// TODO: what if there is more than one base? sdfData only allows
			// for one sdfRef
			for (int j = 0; j < module->ident[i].base_size; j++)
			{
				ident->setReference(identities[module->ident[i].base[j]->name]);
				cout << module->ident[i].base[j]->name << endl;
			}
			tpdfObject->addDatatype(ident);
			identities[module->ident[i].name] = ident;
		}
	}

    int thingcnt = 0;
    int objcnt = 0;
    sdfThing *parentThing = thing;
    sdfThing *childThing;
    sdfObject *childObject;
    sdfProperty *childProperty;
    sdfAction *childAction;
    sdfEvent *childEvent;
	stack<sdfThing*> branchThings;
    // iterate over all child nodes of the module
	struct lys_node *start0 = module->data;
	struct lys_node *elem0;
	LY_TREE_FOR(start0, elem0)
	{
		parentThing = thing;
		childThing = NULL;
		branchThings = {};

		// for the current child of the module elem0 iterate over all
		// via depth first search (dfs)
		struct lys_node *start1 = elem0;
		struct lys_node *elem1;
		struct lys_node *next1;
		LY_TREE_DFS_BEGIN (start1, next1, elem1)
		//do
		{
			// check node type (container, grouping, leaf, etc.)
			if (elem1->nodetype == LYS_CONTAINER)
			{
				childThing = containerToSdfThing((lys_node_container*)elem1);
				parentThing->addThing(childThing);
				thingcnt++;
			}
			else if (elem1->nodetype == LYS_GROUPING)
			{
				childThing = groupingToSdfThing((lys_node_grp*)elem1);
				parentThing->addThing(childThing);
				thingcnt++;
			}
			else if (elem1->nodetype == LYS_NOTIF)
			{
				childObject = new sdfObject(parentThing->getLabel());
				childEvent = notificationToSdfEvent((lys_node_notif*)elem1);
				childObject->addEvent(childEvent);
				parentThing->addObject(childObject);
				objcnt++;
			}
			else if (elem1->nodetype == LYS_RPC)
			{
				childThing = new sdfThing(parentThing->getLabel() + "-action");
				childObject = new sdfObject(parentThing->getLabel() + "-action");
				childAction = rpcToSdfAction((lys_node_rpc_action*)elem1);
				childObject->addAction(childAction);
				parentThing->addThing(childThing);
				childThing->addObject(childObject);
				objcnt++;
			}
			else if (elem1->nodetype == LYS_LEAF)
			{
				childObject = new sdfObject(parentThing->getLabel());
				childProperty = leafToSdfProperty((lys_node_leaf*)elem1);
				childObject->addProperty(childProperty);
				parentThing->addObject(childObject);
				objcnt++;
			}
			else if (elem1->nodetype == LYS_LEAFLIST)
			{
				childObject = new sdfObject(string(elem1->name) + "-object");
				childProperty = leaflistToSdfProperty((lys_node_leaflist*)elem1);
				childObject->addProperty(childProperty);
				parentThing->addObject(childObject);
				objcnt++;
			}
			// TODO: if all child nodes of grouping or container are leafs,
			// turn into object instead of thing?
			/*
			if (elem1->nodetype == LYS_GROUPING
			|| elem1->nodetype == LYS_CONTAINER
			|| elem1->nodetype == LYS_LIST)
			{
				// first check if all children are leafs
				struct lys_node *start2 = elem1->child;
				struct lys_node *elem2;
				bool children_only_leafs = true;
				LY_TREE_FOR (start2, elem2)
				{
					if (elem2->nodetype != LYS_LEAF
					|| elem2->nodetype == LYS_LEAFLIST)
					{
						children_only_leafs = false;
						break;
					}
				}
				// if so the current node is translated to sdfObject
				if (children_only_leafs)
				{
					object = new sdfObject(elem1->name, elem1->dsc);
					parentThing->addObject(object);
					// go deeper for properties etc.?
					struct lys_node *start2 = elem1->child;
					struct lys_node *elem2;
					struct lys_node_leaf *elem_leaf;
					LY_TREE_FOR (start2, elem2)
					{
						elem_leaf = (lys_node_leaf*)elem2;
						property = leafToSdfProperty(elem_leaf);
						object->addProperty(property);
					}

				}
				// translate to sdfThing otherwise
				else
				{
					thing = new sdfThing(elem1->name, elem1->dsc);
					parentThing->addThing(thing);
				}
			}*/
			// if the current node has further sibling nodes the current
			// parent sdfThing is saved on a stack to come back to later
			if (elem1->next != NULL)
			{
				branchThings.push(parentThing);
			}
			// if the current node has no child nodes the last sdfThing
			// is loaded from the stack as parent (dfs goes back up)
			// the sdfThing is popped (but pushed back on in the next loop
			// if there are further sibling nodes)
			if (elem1->child == NULL && !branchThings.empty())
			{
				parentThing = branchThings.top();
				branchThings.pop();
			}
			// if the current node does have child nodes the current parent
			// sdfThing is updated to the next sdfThing in line?
			else if (childThing != NULL && elem1->child != NULL)
			{
				parentThing = childThing;
				// parentObject = object would be unnecessary because
				// sdfObjects cannot have objects themselves and
				// the object should be filled on the spot?
			}
			LY_TREE_DFS_END(start1, next1, elem1);
		}
	}

	cout << "things: " << thingcnt << endl;
	cout << "objects: " << objcnt << endl;
	return thing;
}

// TODO: choice, list (with arrays somehow???), anydata, uses, case,
// inout (?), augment


int main(int argc, char** argv)
{
    // Executed by calling: sdf_converter input_file output_file

    // open specified input file
    std::ifstream input_file;
    std::string input_string;
    input_file.open (argv[1]);
    // read contents of input file
    if (input_file)
    {
        std::ostringstream ss;
        ss << input_file.rdbuf();
        input_string = ss.str();
        input_file.close();
    }
    else
    {
        perror("Error opening file");
        return -1;
    }

    // regexs to check file formats
    std::regex yang_regex (".*\\.yang");
    std::regex sdf_json_regex (".*\\.sdf\\.json");

    // check whether input file is a YANG file
    if (std::regex_match(argv[1], yang_regex))
    {
        // TODO: check whether output file has right format
        if (!std::regex_match(argv[2], sdf_json_regex))
        {
            cerr << "Incorrect output file format\n" << endl;
            //return -1;
        }

        // load the required context
        ly_ctx *ctx = ly_ctx_new("yang/", 0);
        const lys_module *module = lys_parse_path(ctx, argv[1], LYS_IN_YANG);

        if (module == NULL)
        {
        	cerr << "Module parsing failed" << endl;
        	return -1;
        }

        sdfThing *moduleThing = moduleToSdfThing(module);
        moduleThing->thingToFile(argv[2]);
		/*
		//TEST
        sdfObject test("test", "");
        sdfEvent teste("e", "...");
        sdfData testd("d", "...", json_string);
        sdfProperty testp("p", "prop");
        sdfAction testa("testa", "blah");
        test.addAction(&testa);
        test.addEvent(&teste);
        test.addProperty(&testp);
        testa.addDatatype(&testd);
        testp.setReference(&testd);
        teste.addOutputData(&testd);
        testa.addInputData(&testd);
        testa.addRequiredInputData(&testd);

        testd.setStringData("yeah");

        test.objectToFile(argv[2]);
		*/

        return 0;
    }

    // check whether input file is an SDF file
    if (std::regex_match(argv[1], sdf_json_regex))
    {
        // check whether output file has right format
        if (!std::regex_match(argv[2], yang_regex))
        {
            printf("Incorrect output file format\n");
            //return -1;
        }

        const lys_module *module;
        // nlohman to iterate over sdf?
        // fill module
        lys_print_path(argv[2], module, LYS_OUT_YANG, NULL, 0, 0);

        return 0;
    }

    return -1;
}
