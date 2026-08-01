#pragma once

#include <Skirnir/Skirnir.hpp>

class EmptyApp : public skr::IApplication
{
  public:
    explicit EmptyApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

    void Run() override {}
};