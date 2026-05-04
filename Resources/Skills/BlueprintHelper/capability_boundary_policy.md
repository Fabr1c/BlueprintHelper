# Capability Boundary Policy

Asset Factory：只创建资产。
Component：add_component 只创建和 attachment；属性另走 set_component_properties。
Class Settings：只修改类设置，不创建 BPI，不写接口函数 body，不支持第一版 Parent Class 修改。
Enhanced Input：默认不创建或修改 IA / IMC；只引用明确 IA，接入已有执行流必须 Merge + dry_run。
Graph Write：Append 新/空图表独立逻辑；Replace 明确目标完整实现；Patch 精确 path；Merge 接入已有执行流。
