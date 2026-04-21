#pragma once

#include <Skirnir/Skirnir.hpp>

class EmptyApp : public skr::IApplication
{
  public:
    explicit EmptyApp(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

    void Run() override {}
};