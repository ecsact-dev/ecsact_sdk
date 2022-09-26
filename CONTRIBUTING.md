# Contributing

The Ecsact SDK repository doesn't contain very much source code, but is a great place to start contributing to _other ecsact repositories_.

## Getting Started

If you want to makes changes to another ecsact repository (such as [ecsact_rtb](https://github.com/ecsact-dev/ecsact_rtb)) you may want to install the SDK with your changes. To do that you use the `DevInstall` script with the repository you want to contribute to

Add to your `user.bazelrc` file in this repository to override the specified one in the `WORKSPACE.bazel` file. If `user.bazelrc` doesn't exist then create one.

```bazelrc
build --override_repository=ecsact_rtb=/path/to/cloned/repository/repository
```

### DevInstall Script (Windows)

The windows `DevInstall` script creates a `.misx` file and installs it automatically for you. You're required to create to create a cerficiate in this path `%USERPROFILE%\Certificates\EcsactDev.pfx`. We recommend using [MSIX HERO](https://msixhero.net/) to make this easier.

After your certificate is created, run the script and follow the prompts.

```powershell
pwsh .\DevInstall.ps1
```

After your install `ecsact` should be available in your command line. Run `ecsact config` to get a JSON output of your current installation.
