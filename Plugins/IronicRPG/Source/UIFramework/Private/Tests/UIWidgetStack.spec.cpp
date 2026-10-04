// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Widgets/MenuBase.h"
#include "Widgets/UIWidgetStack.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUIWidgetStackMixedVisibilityPolicyTest,
    "IronicRPG.UIFramework.WidgetStack.MixedClosePoliciesPreserveEarlierVisiblePages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUIWidgetStackMixedVisibilityPolicyTest::RunTest(const FString& Parameters)
{
    UUIWidgetStack* Stack = NewObject<UUIWidgetStack>();
    UMenuBase* FirstWidget = NewObject<UMenuBase>();
    UMenuBase* SecondWidget = NewObject<UMenuBase>();
    UMenuBase* ThirdWidget = NewObject<UMenuBase>();

    TestTrue(TEXT("The first page is pushed"), Stack->PushWidget(FirstWidget));
    TestTrue(TEXT("The second page is pushed without closing the first"), Stack->PushWidget(SecondWidget, false));
    TestEqual(TEXT("The first page remains visible"), FirstWidget->GetVisibility(), ESlateVisibility::Visible);
    TestEqual(TEXT("The second page is visible"), SecondWidget->GetVisibility(), ESlateVisibility::Visible);

    TestTrue(TEXT("The third page is pushed and closes only the second"), Stack->PushWidget(ThirdWidget, true));
    TestEqual(TEXT("The first page remains visible behind all later pages"), FirstWidget->GetVisibility(), ESlateVisibility::Visible);
    TestEqual(TEXT("The second page is hidden by the third"), SecondWidget->GetVisibility(), ESlateVisibility::Hidden);
    TestEqual(TEXT("The third page is visible"), ThirdWidget->GetVisibility(), ESlateVisibility::Visible);

    TestTrue(TEXT("The third page is popped"), Stack->PopWidget() == ThirdWidget);
    TestEqual(TEXT("The second page is restored"), SecondWidget->GetVisibility(), ESlateVisibility::Visible);
    TestEqual(TEXT("The first page remains visible after restoring the second"), FirstWidget->GetVisibility(), ESlateVisibility::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUIWidgetStackRemoveHiddenTargetTest,
    "IronicRPG.UIFramework.WidgetStack.RemovingHiddenTargetDoesNotCloseEarlierVisiblePage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUIWidgetStackRemoveHiddenTargetTest::RunTest(const FString& Parameters)
{
    UUIWidgetStack* Stack = NewObject<UUIWidgetStack>();
    UMenuBase* FirstWidget = NewObject<UMenuBase>();
    UMenuBase* SecondWidget = NewObject<UMenuBase>();
    UMenuBase* ThirdWidget = NewObject<UMenuBase>();

    Stack->PushWidget(FirstWidget);
    Stack->PushWidget(SecondWidget, false);
    Stack->PushWidget(ThirdWidget, true);

    TestTrue(TEXT("The hidden second page can be removed"), Stack->RemoveWidget(SecondWidget));
    TestEqual(TEXT("The earlier page remains visible"), FirstWidget->GetVisibility(), ESlateVisibility::Visible);
    TestTrue(TEXT("The third page remains the top page"), Stack->GetTopWidget() == ThirdWidget);

    TestTrue(TEXT("The third page is popped"), Stack->PopWidget() == ThirdWidget);
    TestEqual(TEXT("The earlier page remains visible after the removed target cannot be restored"), FirstWidget->GetVisibility(), ESlateVisibility::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUIWidgetStackPopToWidgetTest,
    "IronicRPG.UIFramework.WidgetStack.PopToWidgetClosesOnlyWidgetsAboveTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUIWidgetStackPopToWidgetTest::RunTest(const FString& Parameters)
{
    UUIWidgetStack* Stack = NewObject<UUIWidgetStack>();
    UMenuBase* FirstWidget = NewObject<UMenuBase>();
    UMenuBase* TargetWidget = NewObject<UMenuBase>();
    UMenuBase* ThirdWidget = NewObject<UMenuBase>();
    UMenuBase* FourthWidget = NewObject<UMenuBase>();

    Stack->PushWidget(FirstWidget);
    Stack->PushWidget(TargetWidget, false);
    Stack->PushWidget(ThirdWidget, true);
    Stack->PushWidget(FourthWidget, false);

    TestTrue(TEXT("The target in the stack is accepted"), Stack->PopToWidget(TargetWidget));
    TestTrue(TEXT("The requested widget becomes the top widget"), Stack->GetTopWidget() == TargetWidget);
    TestEqual(TEXT("Only the target and widgets below it remain"), Stack->GetStackSize(), 2);
    TestEqual(TEXT("The target is restored after its covering widget is popped"), TargetWidget->GetVisibility(), ESlateVisibility::Visible);
    TestEqual(TEXT("An earlier overlaid page remains visible"), FirstWidget->GetVisibility(), ESlateVisibility::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUIWidgetStackPopToMissingWidgetTest,
    "IronicRPG.UIFramework.WidgetStack.PopToMissingWidgetLeavesStackUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUIWidgetStackPopToMissingWidgetTest::RunTest(const FString& Parameters)
{
    UUIWidgetStack* Stack = NewObject<UUIWidgetStack>();
    UMenuBase* FirstWidget = NewObject<UMenuBase>();
    UMenuBase* TopWidget = NewObject<UMenuBase>();
    UMenuBase* MissingWidget = NewObject<UMenuBase>();

    Stack->PushWidget(FirstWidget);
    Stack->PushWidget(TopWidget);

    TestFalse(TEXT("A widget outside the stack is rejected"), Stack->PopToWidget(MissingWidget));
    TestTrue(TEXT("The top widget remains unchanged"), Stack->GetTopWidget() == TopWidget);
    TestEqual(TEXT("No widgets are popped"), Stack->GetStackSize(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUIWidgetStackClearWidgetsTest,
    "IronicRPG.UIFramework.WidgetStack.ClearWidgetsClosesEveryWidget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUIWidgetStackClearWidgetsTest::RunTest(const FString& Parameters)
{
    UUIWidgetStack* Stack = NewObject<UUIWidgetStack>();
    UMenuBase* FirstWidget = NewObject<UMenuBase>();
    UMenuBase* SecondWidget = NewObject<UMenuBase>();
    UMenuBase* ThirdWidget = NewObject<UMenuBase>();

    Stack->PushWidget(FirstWidget);
    Stack->PushWidget(SecondWidget, false);
    Stack->PushWidget(ThirdWidget);
    Stack->ClearWidgets();

    TestTrue(TEXT("The stack is empty"), Stack->IsEmpty());
    TestEqual(TEXT("The stack size is zero"), Stack->GetStackSize(), 0);
    TestNull(TEXT("The first widget is detached"), FirstWidget->GetParent());
    TestNull(TEXT("The second widget is detached"), SecondWidget->GetParent());
    TestNull(TEXT("The third widget is detached"), ThirdWidget->GetParent());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
