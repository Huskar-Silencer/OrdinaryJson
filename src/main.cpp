#include <chrono>
#include <iostream>
#include <vector>

#include "ordinary_json.hpp"

const std::string longJson = R"(
{
    "company": "Tech Corp",
    "founded_year": 2005,
    "is_public": true
}
)";

const std::string flatLongJson = R"(
{
    "field_001": "value_001",
    "field_002": "value_002",
    "field_003": "value_003",
    "field_004": "value_004",
    "field_005": "value_005",
    "field_006": "value_006",
    "field_007": "value_007",
    "field_008": "value_008",
    "field_009": "value_009",
    "field_010": "value_010",
    "field_011": "value_011",
    "field_012": "value_012",
    "field_013": "value_013",
    "field_014": "value_014",
    "field_015": "value_015",
    "field_016": "value_016",
    "field_017": "value_017",
    "field_018": "value_018",
    "field_019": "value_019",
    "field_020": "value_020",
    "field_021": "value_021",
    "field_022": "value_022",
    "field_023": "value_023",
    "field_024": "value_024",
    "field_025": "value_025",
    "field_026": "value_026",
    "field_027": "value_027",
    "field_028": "value_028",
    "field_029": "value_029",
    "field_030": "value_030",
    "field_031": "value_031",
    "field_032": "value_032",
    "field_033": "value_033",
    "field_034": "value_034",
    "field_035": "value_035",
    "field_036": "value_036",
    "field_037": "value_037",
    "field_038": "value_038",
    "field_039": "value_039",
    "field_040": "value_040",
    "field_041": "value_041",
    "field_042": "value_042",
    "field_043": "value_043",
    "field_044": "value_044",
    "field_045": "value_045",
    "field_046": "value_046",
    "field_047": "value_047",
    "field_048": "value_048",
    "field_049": "value_049",
    "field_050": "value_050",
    "field_051": "value_051",
    "field_052": "value_052",
    "field_053": "value_053",
    "field_054": "value_054",
    "field_055": "value_055",
    "field_056": "value_056",
    "field_057": "value_057",
    "field_058": "value_058",
    "field_059": "value_059",
    "field_060": "value_060",
    "field_061": "value_061",
    "field_062": "value_062",
    "field_063": "value_063",
    "field_064": "value_064",
    "field_065": "value_065",
    "field_066": "value_066",
    "field_067": "value_067",
    "field_068": "value_068",
    "field_069": "value_069",
    "field_070": "value_070",
    "field_071": "value_071",
    "field_072": "value_072",
    "field_073": "value_073",
    "field_074": "value_074",
    "field_075": "value_075",
    "field_076": "value_076",
    "field_077": "value_077",
    "field_078": "value_078",
    "field_079": "value_079",
    "field_080": "value_080",
    "field_081": "value_081",
    "field_082": "value_082",
    "field_083": "value_083",
    "field_084": "value_084",
    "field_085": "value_085",
    "field_086": "value_086",
    "field_087": "value_087",
    "field_088": "value_088",
    "field_089": "value_089",
    "field_090": "value_090",
    "field_091": "value_091",
    "field_092": "value_092",
    "field_093": "value_093",
    "field_094": "value_094",
    "field_095": "value_095",
    "field_096": "value_096",
    "field_097": "value_097",
    "field_098": "value_098",
    "field_099": "value_099",
    "field_100": "value_100"
}
)";

void mineFunction() {
    auto start = std::chrono::high_resolution_clock::now();
    ordinaryjson::OrdinaryJsonNode node = ordinaryjson::Parse(flatLongJson);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "time: "
            << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count()
            << " ns" << std::endl;
    std::cout << node.GetAsObject("field_099").GetAsString() << std::endl;
}

int main() {
    std::string json = R"({
    "app": {"name": "myservice", "debug": false, "ports": [8080, 8081]}
  })";

    ordinaryjson::OrdinaryJsonNode node = ordinaryjson::Parse(json);

    // read
    std::cout << node.GetAsObject("app").GetAsObject("name").GetAsString() << "\n";

    // iterate
    for (auto &kv: node.GetAsObject("app").GetObjectNodeRef()) {
        std::cout << "  " << kv.first << " : "
                << kv.second.GetValueTypeToString() << "\n";
    }

    // modify
    node.GetAsObject("app").GetAsObject("debug").GetAsBool() = true;
    node.GetAsObject("app").GetAsObject("ports").GetArrayNodeRef().push_back(
        ordinaryjson::OrdinaryJsonNode(9090));

    // serialize
    std::cout << node.Stringify() << "\n";
    return 0;
}
