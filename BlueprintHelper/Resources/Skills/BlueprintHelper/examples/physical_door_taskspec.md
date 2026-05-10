# Example: Physical Door TaskSpec-first

不要直接调用 add_component / set_component_properties / add_interface / append_graph 序列。

流程：

```text
get_runtime_profile
read_task_context(target=/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor)
build TaskSpec(feature_name=PhysicsDoor)
preview_task
execute_task
report summary
```

TaskSpec 中表达：组件、组件属性、接口、变量、EG_PhysicsDoor、Custom Events、validation。Task Compiler 负责生成 TaskPlan，UE Task Runtime 调用内部 capability。
