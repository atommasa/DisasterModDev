// Copyright Ironic Studio. All Rights Reserved.


#include "Decorators/RPGTextBlockDecorator.h"
#include "Decorators/RPGTextDecoratorInstance.h"
#include "Components/RichTextBlock.h"

TSharedPtr<ITextDecorator> URPGTextBlockDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShareable(new FRPGTextStyleDecorator(InOwner));
}
