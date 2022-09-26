# Contributing

The Ecsact SDK repository doesn't contain very much source code, but is a great place to start contributing to _other ecsact repositories_.

## Getting Started

If you want to makes changes to another ecsact repository (such as [ecsact_rtb](https://github.com/ecsact-dev/ecsact_rtb)) you may want to install the SDK with your changes. To do that you use the `DevInstall` script with the repository you want to contribute to

The Ecsact SDK is built with [`bazel`](https://bazel.build/). We recommend you install [bazelisk](https://bazel.build/install/bazelisk) and have it available as `bazel` in your `PATH`.

To configure your `bazel` settings you may add options to your `user.bazelrc` file in the root of this repository. If `user.bazelrc` doesn't exist, create one.

Add [override_repository](https://bazel.build/reference/command-line-reference#flag--override_repository) options for repositories you want to contribute to:

```bazelrc
build --override_repository=example1=/path/to/cloned/example1_repo
build --override_repository=example2=/path/to/cloned/example2_repo
```

Optionally if you'd like to build everything in **debug mode** you may add the following:

```bazelrc
build -c dbg
```

### DevInstall Script (Windows)

The windows `DevInstall` script creates a `.misx` file and installs it automatically for you. You're required to create to create a cerficiate in this path `%USERPROFILE%\Certificates\EcsactDev.pfx`. We recommend using [MSIX HERO](https://msixhero.net/) to make this easier.

After your certificate is created, run the script and follow the prompts.

```powershell
pwsh .\DevInstall.ps1
```

After your install `ecsact` should be available in your command line. Run `ecsact config` to get a JSON output of your current installation.
