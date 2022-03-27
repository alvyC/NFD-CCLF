Vagrant.configure("2") do |config|
    config.vm.box = "bento/ubuntu-20.04"
    config.vm.provider "virtualbox" do |vb|
        vb.memory = 48000
        vb.cpus = 16
        vb.name = "ndn-hack"
    end
    config.vm.synced_folder ".", "/vagrant", type: "virtualbox"
end